namespace BridgeDotNetWorker;

/// <summary>
/// Stub connector used in CI and when no real T4 SDK is available.
/// All operations return canned "OK" responses for local smoke-testing.
/// </summary>
public sealed class StubT4Connector : IT4Connector
{
    public string Ping() => "OK PONG";

    public string Connect(BridgeConfig cfg) =>
        $"OK stub connected (host={cfg.T4Host}:{cfg.T4Port} user={cfg.T4Username})";

    public string PlaceOrder(string symbol, string side, int quantity, decimal price, string orderType) =>
        $"OK stub order placed: {symbol} {side} {quantity}@{price} type={orderType}";

    public string CancelOrders(string symbol, string account) =>
        $"OK stub cancelled all resting orders: {symbol}" + (string.IsNullOrEmpty(account) ? "" : $" account={account}");
}
