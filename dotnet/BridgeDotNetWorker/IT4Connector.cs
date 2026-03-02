namespace BridgeDotNetWorker;

/// <summary>
/// Abstraction over a T4 trading API connection.
/// All methods return a string starting with "OK" on success or "ERROR ..." on failure.
/// </summary>
public interface IT4Connector
{
    /// <summary>Verifies the connector is alive. Returns "OK PONG".</summary>
    string Ping();

    /// <summary>Connects/logs in to the T4 server using the supplied config.</summary>
    string Connect(BridgeConfig cfg);

    /// <summary>Places an order. Returns "OK &lt;orderId&gt;" or "ERROR ...".</summary>
    string PlaceOrder(string symbol, string side, int quantity, decimal price, string orderType);

    /// <summary>Cancels all resting orders for the given symbol/account. Returns "OK ..." or "ERROR ...".</summary>
    string CancelOrders(string symbol, string account);
}
