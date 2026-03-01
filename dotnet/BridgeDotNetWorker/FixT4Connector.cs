using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Text;

namespace BridgeDotNetWorker;

/// <summary>
/// FIX 4.2 connector for T4 simulator (uhfix-sim.t4login.com:10443).
/// Establishes a TLS 1.2+ connection and performs a FIX 4.2 Logon handshake.
/// Implements only the minimal subset: Logon (35=A), Logout (35=5),
/// Heartbeat (35=0), and TestRequest (35=1).
/// Never sends unsupported message types (e.g. 35=x SecurityListRequest).
/// Never logs password or license key.
/// </summary>
public sealed class FixT4Connector : IT4Connector, IDisposable
{
    private const byte SOH = 0x01;
    private const string BeginString = "FIX.4.2";

    // Session identifiers populated on Connect()
    private string _senderCompId = string.Empty;
    private string _targetCompId = string.Empty;
    private string _senderSubId  = string.Empty;
    private string _targetSubId  = string.Empty;

    private TcpClient?  _tcp;
    private SslStream?  _ssl;
    private int         _seqNum = 1;
    private bool        _connected;
    private bool        _disposed;

    // ── Public API ────────────────────────────────────────────────────────────

    /// <summary>Sends a FIX TestRequest (35=1) and returns "OK PONG" on success.</summary>
    public string Ping()
    {
        if (!_connected || _ssl is null)
            return "ERROR not connected";
        try
        {
            byte[] req = BuildMessage("1", new[] { (112, "PING") });
            _ssl.Write(req);
            Console.WriteLine("[FixT4Connector] >> TestRequest (35=1)");

            string? reply = ReadFIXMessage(timeoutMs: 10_000);
            if (reply is null)
                return "ERROR no response to TestRequest";

            string msgType = GetField(reply, 35);
            Console.WriteLine($"[FixT4Connector] << 35={msgType}");
            // Accept Heartbeat (35=0) or TestRequest echo (35=1)
            return msgType is "0" or "1" ? "OK PONG" : $"OK PONG (35={msgType})";
        }
        catch (Exception ex)
        {
            return $"ERROR {ex.Message}";
        }
    }

    /// <summary>
    /// Connects to the T4 FIX endpoint, authenticates with FIX 4.2 Logon, and
    /// returns "OK connected" on success or "ERROR …" on failure.
    /// </summary>
    public string Connect(BridgeConfig cfg)
    {
        if (string.IsNullOrEmpty(cfg.T4Username))
            return "ERROR T4Username is required (set T4_USERNAME env var)";
        if (string.IsNullOrEmpty(cfg.T4Password))
            return "ERROR T4Password is required (set T4_PASSWORD env var)";

        try
        {
            // Cache session identifiers (no secrets)
            _senderCompId = string.IsNullOrEmpty(cfg.FixSenderCompID) ? cfg.T4Username : cfg.FixSenderCompID;
            _targetCompId = cfg.FixTargetCompID;
            _senderSubId  = cfg.FixSenderSubID;
            _targetSubId  = cfg.FixTargetSubID;

            Console.WriteLine($"[FixT4Connector] Connecting to {cfg.T4Host}:{cfg.T4Port} …");

            // Establish TCP + TLS
            _tcp = new TcpClient();
            _tcp.Connect(cfg.T4Host, cfg.T4Port);

            _ssl = new SslStream(_tcp.GetStream(), leaveInnerStreamOpen: false,
                userCertificateValidationCallback: null);
            _ssl.AuthenticateAsClient(cfg.T4Host, null,
                SslProtocols.Tls12 | SslProtocols.Tls13,
                checkCertificateRevocation: false);

            Console.WriteLine($"[FixT4Connector] TLS handshake OK ({_ssl.SslProtocol}).");

            _seqNum    = 1;
            _connected = false;

            // Send FIX 4.2 Logon (35=A)
            // Tags 553=Username / 554=Password are non-standard in FIX 4.2 but
            // widely supported by T4 and other venues.
            var logonFields = new List<(int, string)>
            {
                (98,  "0"),                                        // EncryptMethod = None
                (108, cfg.FixHeartBtInt.ToString()),               // HeartBtInt
                (553, cfg.T4Username),                             // Username
                (554, cfg.T4Password!),                            // Password (never logged)
            };

            // Include license key in tag 96 (RawData) when provided, which some
            // T4 deployments use to pass a license string during Logon.
            if (!string.IsNullOrEmpty(cfg.T4LicenseKey))
                logonFields.Add((96, cfg.T4LicenseKey));

            byte[] logon = BuildMessage("A", logonFields);
            _ssl.Write(logon);
            Console.WriteLine("[FixT4Connector] >> Logon (35=A)");

            // Wait for server's Logon response (or Logout / Reject)
            for (int attempts = 0; attempts < 5; attempts++)
            {
                string? reply = ReadFIXMessage(timeoutMs: 15_000);
                if (reply is null)
                    return "ERROR FIX Logon timed out (15 s)";

                string msgType = GetField(reply, 35);
                Console.WriteLine($"[FixT4Connector] << 35={msgType}");

                switch (msgType)
                {
                    case "A":
                        _connected = true;
                        Console.WriteLine("[FixT4Connector] Logon accepted.");
                        return "OK connected";

                    case "5":
                        string logoutText = GetField(reply, 58);
                        return $"ERROR FIX Logout: {logoutText}";

                    case "3":
                        string rejectText    = GetField(reply, 58);
                        string refMsgType    = GetField(reply, 372);
                        string refTagId      = GetField(reply, 371);
                        return $"ERROR FIX Session Reject: {rejectText} (RefMsgType={refMsgType} RefTagID={refTagId})";

                    case "0":
                    case "1":
                        // Heartbeat or TestRequest before Logon – respond and keep waiting
                        HandleAdminMessage(msgType, reply);
                        break;

                    default:
                        // Unexpected – log and keep waiting
                        Console.WriteLine($"[FixT4Connector] Unexpected 35={msgType} during Logon; waiting …");
                        break;
                }
            }

            return "ERROR FIX Logon: no Logon response received";
        }
        catch (Exception ex)
        {
            return $"ERROR exception during FIX connect: {ex.Message}";
        }
    }

    /// <summary>Not yet implemented for FIX connector.</summary>
    public string PlaceOrder(string symbol, string side, int quantity, decimal price, string orderType)
        => "ERROR FIX order placement not yet implemented";

    // ── IDisposable ───────────────────────────────────────────────────────────

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        bool wasConnected = _connected;
        _connected = false;
        try
        {
            if (_ssl is not null && wasConnected)
            {
                // Send Logout (35=5) before closing
                try
                {
                    byte[] logout = BuildMessage("5", Array.Empty<(int, string)>());
                    _ssl.Write(logout);
                    Console.WriteLine("[FixT4Connector] >> Logout (35=5)");
                }
                catch { /* best-effort */ }
            }
            _ssl?.Dispose();
            _tcp?.Dispose();
        }
        catch { /* best-effort cleanup */ }
    }

    // ── Private helpers ───────────────────────────────────────────────────────

    /// <summary>
    /// Responds to administrative messages received during the Logon sequence.
    /// </summary>
    private void HandleAdminMessage(string msgType, string message)
    {
        if (msgType == "1") // TestRequest – must reply with Heartbeat (35=0)
        {
            string testReqId = GetField(message, 112);
            byte[] hb = BuildMessage("0", new[] { (112, testReqId) });
            _ssl?.Write(hb);
            Console.WriteLine($"[FixT4Connector] >> Heartbeat (35=0) in reply to TestRequest id={testReqId}");
        }
    }

    /// <summary>
    /// Reads bytes from the TLS stream until a complete FIX message is received
    /// (i.e. the last field is the checksum tag 10).
    /// Returns null on timeout or stream closure.
    /// </summary>
    private string? ReadFIXMessage(int timeoutMs)
    {
        if (_tcp is null || _ssl is null)
            return null;

        _tcp.ReceiveTimeout = timeoutMs;

        var buf = new List<byte>(512);
        try
        {
            while (true)
            {
                int b = _ssl.ReadByte();
                if (b < 0) return null; // EOF

                buf.Add((byte)b);

                // FIX messages end when the last complete field is 10=NNN
                if (b == SOH && buf.Count >= 8)
                {
                    // Find start of the last field (just after the previous SOH)
                    int end = buf.Count - 1; // index of the terminating SOH
                    int start = end - 1;
                    while (start > 0 && buf[start - 1] != SOH)
                        start--;

                    int fieldLen = end - start;
                    // "10=" is 3 bytes minimum, followed by 3-digit checksum = at least 6 chars
                    if (fieldLen >= 4 &&
                        buf[start] == (byte)'1' &&
                        buf[start + 1] == (byte)'0' &&
                        buf[start + 2] == (byte)'=')
                    {
                        return Encoding.ASCII.GetString(buf.ToArray());
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[FixT4Connector] ReadFIXMessage error: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// Builds a FIX 4.2 message byte array with the correct BodyLength and CheckSum.
    /// </summary>
    private byte[] BuildMessage(string msgType, IEnumerable<(int tag, string value)> bodyFields)
    {
        // Build body (fields 35 onwards, up to but not including checksum)
        var body = new StringBuilder();
        Append(body, 35, msgType);
        Append(body, 49, _senderCompId);
        Append(body, 56, _targetCompId);
        if (!string.IsNullOrEmpty(_senderSubId))
            Append(body, 50, _senderSubId);
        if (!string.IsNullOrEmpty(_targetSubId))
            Append(body, 57, _targetSubId);
        Append(body, 34, (_seqNum++).ToString());
        Append(body, 52, DateTime.UtcNow.ToString("yyyyMMdd-HH:mm:ss.fff"));

        foreach (var (tag, value) in bodyFields)
            Append(body, tag, value);

        string bodyStr = body.ToString();
        int bodyLen = Encoding.ASCII.GetByteCount(bodyStr);

        // Header
        string header = $"8={BeginString}\x0109={bodyLen}\x01";
        string full   = header + bodyStr;

        // CheckSum = sum of all bytes mod 256, formatted as 3 digits
        byte[] fullBytes = Encoding.ASCII.GetBytes(full);
        int checksum = 0;
        foreach (byte byt in fullBytes)
            checksum = (checksum + byt) & 0xFF;

        string trailer = $"10={checksum:D3}\x01";
        return Encoding.ASCII.GetBytes(full + trailer);
    }

    private static void Append(StringBuilder sb, int tag, string value)
    {
        sb.Append(tag);
        sb.Append('=');
        sb.Append(value);
        sb.Append('\x01');
    }

    /// <summary>
    /// Extracts the value of the given FIX tag from a raw FIX message string.
    /// FIX fields are delimited by SOH (ASCII 0x01).
    /// </summary>
    internal static string GetField(string message, int tag)
    {
        // Try \x01tag= first (field is not at the start)
        string prefix = $"\x01{tag}=";
        int idx = message.IndexOf(prefix, StringComparison.Ordinal);
        if (idx >= 0)
        {
            int start = idx + prefix.Length;
            int end   = message.IndexOf('\x01', start);
            return end >= 0 ? message[start..end] : message[start..];
        }

        // Field may be at the very start (e.g. BeginString tag 8)
        string altPrefix = $"{tag}=";
        if (message.StartsWith(altPrefix, StringComparison.Ordinal))
        {
            int start = altPrefix.Length;
            int end   = message.IndexOf('\x01', start);
            return end >= 0 ? message[start..end] : message[start..];
        }

        return string.Empty;
    }
}
