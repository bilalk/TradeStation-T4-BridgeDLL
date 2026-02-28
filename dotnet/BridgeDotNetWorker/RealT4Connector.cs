namespace BridgeDotNetWorker;

/// <summary>
/// Real T4 connector. Requires the CTS.T4API NuGet package and building with /p:T4SDK=true.
/// Never logs password or license key.
/// See docs/Build_and_Run.md → "Enabling the Real T4 Connector" for setup instructions.
/// </summary>
public sealed class RealT4Connector : IT4Connector
{
#if REAL_T4_SDK
    // ---------------------------------------------------------------------------
    // NOTE: The T4.API namespace below refers to the CTS Futures CTS.T4API NuGet package.
    // Add the package and build with /p:T4SDK=true to activate this code path.
    // ---------------------------------------------------------------------------
    private T4.API.Host? _host;
    private bool _connected;
    private string _lastError = string.Empty;
    private readonly System.Threading.ManualResetEventSlim _loginEvent = new(false);

    public string Ping() => _connected ? "OK PONG" : "ERROR not connected";

    public string Connect(BridgeConfig cfg)
    {
        if (string.IsNullOrEmpty(cfg.T4Username))
            return "ERROR T4Username is required";
        if (string.IsNullOrEmpty(cfg.T4Password))
            return "ERROR T4Password is required (set T4_PASSWORD env var)";
        if (string.IsNullOrEmpty(cfg.T4Host))
            return "ERROR T4Host is required (set T4_HOST env var)";

        try
        {
            _loginEvent.Reset();
            _connected = false;
            _lastError = string.Empty;

            // Auto-detect simulator vs production based on host name.
            T4.APIServerType serverType = cfg.T4Host.Contains("sim", StringComparison.OrdinalIgnoreCase)
                ? T4.APIServerType.Simulator
                : T4.APIServerType.Production;

            _host = new T4.API.Host(
                serverType,
                "BridgeDotNetWorker",
                cfg.T4LicenseKey ?? string.Empty,
                cfg.T4Firm ?? string.Empty,
                cfg.T4Username,
                cfg.T4Password
            );
            _host.LoginResponse += OnLoginResponse;

            // Wait up to 15 s for login callback
            bool signalled = _loginEvent.Wait(TimeSpan.FromSeconds(15));
            if (!signalled)
                return "ERROR login timed out (15 s)";

            return _connected ? "OK connected" : $"ERROR {_lastError}";
        }
        catch (Exception ex)
        {
            return $"ERROR exception during connect: {ex.Message}";
        }
    }

    public string PlaceOrder(string symbol, string side, int quantity, decimal price, string orderType)
    {
        if (!_connected || _host is null)
            return "ERROR not connected";
        try
        {
            // Placeholder: real order submission via T4.API would go here.
            // Return a canned response until the order-routing integration is complete.
            return $"OK order submitted (stub path): {symbol} {side} {quantity}@{price} {orderType}";
        }
        catch (Exception ex)
        {
            return $"ERROR {ex.Message}";
        }
    }

    private void OnLoginResponse(T4.API.Host host, T4.API.LoginResponseEventArgs args)
    {
        if (args.Result == T4.LoginResult.Accepted)
        {
            _connected = true;
        }
        else
        {
            _connected = false;
            _lastError = args.Result.ToString();
        }
        _loginEvent.Set();
    }
#else
    // ---------------------------------------------------------------------------
    // SDK not available in this build – fail fast with an actionable message.
    // ---------------------------------------------------------------------------
    private const string SdkNotAvailable =
        "ERROR REAL connector not available in this build. " +
        "Install CTS.T4API NuGet package and rebuild with /p:T4SDK=true. " +
        "See docs/Build_and_Run.md for instructions.";

    public string Ping()                                                             => SdkNotAvailable;
    public string Connect(BridgeConfig cfg)                                          => SdkNotAvailable;
    public string PlaceOrder(string s, string sd, int q, decimal p, string ot)      => SdkNotAvailable;
#endif
}
