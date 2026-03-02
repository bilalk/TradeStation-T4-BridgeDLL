using System.Net.Security;
using System.Net.Sockets;
using System.Text;

namespace BridgeDotNetWorker;

/// <summary>
/// FIX 4.2 connector that communicates with the T4 simulator over TLS.
/// Logon uses Tag 91 (RawData) to carry the license key, as required by T4.
/// Never logs password or license key.
/// </summary>
public sealed class FixT4Connector : IT4Connector, IDisposable
{
    private const char   SOH           = '\x01';
    private const string TargetCompId  = "CTS";

    private TcpClient?    _client;
    private SslStream?    _ssl;
    private int           _seqNum       = 1;
    private string        _senderCompId = string.Empty;
    private bool          _connected;
    private bool          _disposed;

    // ── IT4Connector ───────────────────────────────────────────────────────────

    public string Ping() => _connected ? "OK PONG" : "ERROR not connected";

    public string Connect(BridgeConfig cfg)
    {
        if (string.IsNullOrEmpty(cfg.T4Username))
            return "ERROR T4Username is required (set T4_USERNAME env var)";
        if (string.IsNullOrEmpty(cfg.T4Password))
            return "ERROR T4Password is required (set T4_PASSWORD env var)";
        if (string.IsNullOrEmpty(cfg.T4LicenseKey))
            return "ERROR T4LicenseKey is required (set T4_LICENSE_KEY env var)";

        try
        {
            _senderCompId = cfg.T4Username;
            _seqNum = 1;

            Console.WriteLine($"[FixT4Connector] Connecting to {cfg.T4Host}:{cfg.T4Port}…");
            _client = new TcpClient();
            _client.Connect(cfg.T4Host, cfg.T4Port);

            // TLS – accept self-signed certs on simulator
            _ssl = new SslStream(_client.GetStream(), leaveInnerStreamOpen: false,
                (_, _, _, _) => true);
            _ssl.AuthenticateAsClient(cfg.T4Host);
            Console.WriteLine("[FixT4Connector] TLS handshake complete");

            Console.WriteLine("[FixT4Connector] Sending Logon (35=A) with Tag 91 (License)…");
            string logonMsg = BuildLogon(cfg);
            byte[] logonBytes = Encoding.ASCII.GetBytes(logonMsg);
            _ssl.Write(logonBytes, 0, logonBytes.Length);
            _ssl.Flush();

            string? response = ReadMessage(TimeSpan.FromSeconds(15));
            if (response is null)
                return "ERROR Logon timed out";

            string msgType = GetTag(response, 35);
            if (msgType == "A")
            {
                _connected = true;
                Console.WriteLine("[FixT4Connector] Received Logon response (35=A) - Session accepted");
                Console.WriteLine("[FixT4Connector] Connected successfully");
                return "OK connected";
            }

            string text = GetTag(response, 58);
            return msgType == "5"
                ? $"ERROR Logout received: {text}"
                : $"ERROR Unexpected response MsgType={msgType} text={text}";
        }
        catch (Exception ex)
        {
            return $"ERROR {ex.Message}";
        }
    }

    public string PlaceOrder(string symbol, string side, int quantity, decimal price, string orderType)
    {
        if (!_connected || _ssl is null)
            return "ERROR not connected";

        try
        {
            // FIX 4.2 NewOrderSingle (35=D)
            string clOrdId  = $"ORD-{DateTime.UtcNow:yyyyMMddHHmmssfff}";
            char   fixSide  = side.ToUpperInvariant() == "BUY" ? '1' : '2';
            char   ordType  = orderType.ToUpperInvariant() == "MARKET" ? '1' : '2';
            string transTime = DateTime.UtcNow.ToString("yyyyMMdd-HH:mm:ss");

            var body = new StringBuilder();
            body.Append($"11={clOrdId}{SOH}");
            body.Append($"55={symbol}{SOH}");
            body.Append($"54={fixSide}{SOH}");
            body.Append($"38={quantity}{SOH}");
            body.Append($"40={ordType}{SOH}");
            if (ordType == '2')
                body.Append($"44={price:0.##########}{SOH}");
            body.Append($"60={transTime}{SOH}");

            Send(BuildMessage("D", body.ToString()));

            // Try to receive immediate execution report
            string? resp = ReadMessage(TimeSpan.FromSeconds(5));
            if (resp is not null)
            {
                string ordId = GetTag(resp, 37);
                return string.IsNullOrEmpty(ordId)
                    ? $"OK order submitted: {clOrdId}"
                    : $"OK order submitted: {ordId}";
            }
            return $"OK order submitted: {clOrdId}";
        }
        catch (Exception ex)
        {
            return $"ERROR {ex.Message}";
        }
    }

    public string CancelOrders(string symbol, string account)
    {
        if (!_connected || _ssl is null)
            return "ERROR not connected";

        try
        {
            // FIX 4.2 OrderCancelRequest (35=F) – cancel all resting orders for symbol
            string clOrdId   = $"CXL-{DateTime.UtcNow:yyyyMMddHHmmssfff}";
            string transTime = DateTime.UtcNow.ToString("yyyyMMdd-HH:mm:ss");

            var body = new StringBuilder();
            body.Append($"11={clOrdId}{SOH}");
            body.Append($"41={clOrdId}{SOH}");   // OrigClOrdID (same – cancel-all pattern)
            body.Append($"55={symbol}{SOH}");     // Symbol
            body.Append($"54=1{SOH}");            // Side (1=Buy placeholder; server cancels all sides)
            body.Append($"60={transTime}{SOH}");  // TransactTime

            Send(BuildMessage("F", body.ToString()));
            return "OK cancel request sent";
        }
        catch (Exception ex)
        {
            return $"ERROR {ex.Message}";
        }
    }

    // ── IDisposable ────────────────────────────────────────────────────────────

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;

        if (_connected && _ssl is not null)
        {
            try { Send(BuildMessage("5", string.Empty)); }  // Logout (35=5)
            catch { /* best-effort */ }
        }

        _ssl?.Dispose();
        _client?.Dispose();
    }

    // ── FIX helpers ────────────────────────────────────────────────────────────

    private string BuildLogon(BridgeConfig cfg)
    {
        // Tag 95 (RawDataLength) must precede Tag 91 (RawData) per FIX 4.2 spec.
        int licenseLen = Encoding.ASCII.GetByteCount(cfg.T4LicenseKey!);

        var body = new StringBuilder();
        body.Append($"553={cfg.T4Username}{SOH}");   // Username
        body.Append($"554={cfg.T4Password}{SOH}");   // Password (never logged)
        body.Append($"95={licenseLen}{SOH}");         // RawDataLength
        body.Append($"91={cfg.T4LicenseKey}{SOH}");  // RawData / license key
        body.Append($"108=30{SOH}");                  // HeartBtInt (30 s)

        return BuildMessage("A", body.ToString());
    }

    private string BuildMessage(string msgType, string body)
    {
        string sendingTime = DateTime.UtcNow.ToString("yyyyMMdd-HH:mm:ss.fff");

        // Standard header fields (after BeginString and BodyLength)
        var header = new StringBuilder();
        header.Append($"35={msgType}{SOH}");
        header.Append($"49={_senderCompId}{SOH}");
        header.Append($"56={TargetCompId}{SOH}");
        header.Append($"34={_seqNum++}{SOH}");
        header.Append($"52={sendingTime}{SOH}");

        string fullBody    = header.ToString() + body;
        int    bodyLength  = Encoding.ASCII.GetByteCount(fullBody);

        string preChecksum = $"8=FIX.4.2{SOH}9={bodyLength}{SOH}{fullBody}";

        // Checksum = sum of all byte values mod 256
        int checksum = 0;
        foreach (byte b in Encoding.ASCII.GetBytes(preChecksum))
            checksum += b;
        checksum %= 256;

        return $"{preChecksum}10={checksum:D3}{SOH}";
    }

    private void Send(string fixMessage)
    {
        if (_ssl is null) throw new InvalidOperationException("Not connected");
        byte[] bytes = Encoding.ASCII.GetBytes(fixMessage);
        _ssl.Write(bytes, 0, bytes.Length);
        _ssl.Flush();
    }

    /// <summary>
    /// Reads bytes from the SSL stream until one complete FIX message is received
    /// (identified by the terminating <c>10=NNN\x01</c> checksum field), or until
    /// <paramref name="timeout"/> elapses.
    /// </summary>
    private string? ReadMessage(TimeSpan timeout)
    {
        if (_ssl is null) return null;

        _ssl.ReadTimeout = (int)timeout.TotalMilliseconds;
        var    received = new StringBuilder();
        byte[] buffer   = new byte[4096];

        try
        {
            while (true)
            {
                int n = _ssl.Read(buffer, 0, buffer.Length);
                if (n == 0) return null;  // stream closed

                received.Append(Encoding.ASCII.GetString(buffer, 0, n));
                string msg = received.ToString();

                // A complete message ends with \x0110=NNN\x01
                int csIdx = msg.LastIndexOf($"{SOH}10=", StringComparison.Ordinal);
                if (csIdx >= 0)
                {
                    int endIdx = msg.IndexOf(SOH, csIdx + 4);
                    if (endIdx >= 0)
                        return msg[..(endIdx + 1)];
                }
            }
        }
        catch (IOException) { /* read timeout */ }
        catch (Exception ex) { Console.Error.WriteLine($"[FixT4Connector] Read error: {ex.Message}"); }

        return null;
    }

    /// <summary>Extracts the value of a FIX tag from a raw FIX message string.</summary>
    private static string GetTag(string message, int tag)
    {
        // Tags other than the first are preceded by SOH; tag 8 starts the message.
        string delimiter = tag == 8 ? string.Empty : $"{SOH}";
        string prefix    = $"{delimiter}{tag}=";

        int idx = message.IndexOf(prefix, StringComparison.Ordinal);
        if (idx < 0) return string.Empty;

        int valueStart = idx + prefix.Length;
        int valueEnd   = message.IndexOf(SOH, valueStart);
        return valueEnd >= 0
            ? message[valueStart..valueEnd]
            : message[valueStart..];
    }
}
