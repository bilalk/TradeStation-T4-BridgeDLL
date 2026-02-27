namespace BridgeDotNetWorker;

/// <summary>
/// Creates the appropriate <see cref="IT4Connector"/> based on configuration.
/// </summary>
public static class ConnectorFactory
{
    /// <summary>
    /// Returns a connector for the type named in <paramref name="cfg"/>.
    /// If "REAL" is requested but the SDK is unavailable, the returned connector will
    /// respond with a clear error message on every call (fail-fast behaviour).
    /// </summary>
    public static IT4Connector Create(BridgeConfig cfg) =>
        cfg.Connector.ToUpperInvariant() switch
        {
            "REAL" => new RealT4Connector(),
            _      => new StubT4Connector()
        };
}
