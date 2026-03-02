namespace BridgeDotNetWorker;

/// <summary>
/// Real T4 connector via CTS.T4API NuGet package.
/// DEPRECATED in favour of FixT4Connector — see docs/Build_and_Run.md.
/// Never logs password or license key.
/// </summary>
public sealed class RealT4Connector : IT4Connector
{
    // ---------------------------------------------------------------------------
    // SDK not available / deprecated – fail fast with an actionable message.
    // Use BRIDGE_CONNECTOR=FIX and FixT4Connector for real T4 connectivity.
    // ---------------------------------------------------------------------------
    private const string SdkNotAvailable =
        "ERROR REAL connector is deprecated. " +
        "Set BRIDGE_CONNECTOR=FIX and use the native FIX 4.2 connector instead. " +
        "See docs/Build_and_Run.md for instructions.";

    public string Ping()                                                         => SdkNotAvailable;
    public string Connect(BridgeConfig cfg)                                      => SdkNotAvailable;
    public string PlaceOrder(string s, string sd, int q, decimal p, string ot)  => SdkNotAvailable;

#if false // ARCHIVED: CTS.T4API NuGet path — kept for reference only.
#if REAL_T4_SDK
    private T4.API.Host? _host;
    private bool _connected;
    private string _lastError = string.Empty;
    private readonly System.Threading.ManualResetEventSlim _loginEvent = new(false);

    private void OnLoginResponse(T4.API.Host host, T4.API.LoginResponseEventArgs args)
    {
        if (args.Result == T4.LoginResult.Accepted)
            _connected = true;
        else
        {
            _connected = false;
            _lastError = args.Result.ToString();
        }
        _loginEvent.Set();
    }
#endif
#endif
}