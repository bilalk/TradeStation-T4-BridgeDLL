namespace BridgeDotNetWorker;

/// <summary>
/// Abstraction over the T4 / FIX connectivity library.
/// Implementations: StubT4Connector (always available), and – once the
/// T4.Api NuGet package is confirmed – RealT4Connector.
/// </summary>
public interface IT4Connector
{
    /// <summary>Connect and log in to the T4 simulator.</summary>
    /// <returns>A result string starting with "OK" on success or "ERROR …" on failure.</returns>
    Task<string> ConnectAsync(WorkerConfig cfg, CancellationToken ct);

    /// <summary>Place an order.  Fields are passed as a whitespace-delimited string.</summary>
    Task<string> PlaceOrderAsync(string fields, CancellationToken ct);
}

/// <summary>
/// Stub implementation used when the CTS T4.Api NuGet package is not yet
/// available or its exact API is under investigation.
///
/// Behaviour:
///   - ConnectAsync: logs the credentials it *would* use and returns "OK (stub)"
///   - PlaceOrderAsync: logs the order and returns "OK (stub)"
///
/// Next steps to implement real connectivity:
///   1. Add NuGet reference: T4.Api (https://www.nuget.org/profiles/CTSFutures)
///   2. Replace this class with RealT4Connector that instantiates
///      CTS.T4.API.Host, calls Host.Login(), subscribes to Host.OnLoginComplete,
///      and forwards order requests via Market.SubmitOrder / Order.Send.
///   3. Tested packages as of 2026:  T4.Api 6.x or later.
/// </summary>
public class StubT4Connector : IT4Connector
{
    public Task<string> ConnectAsync(WorkerConfig cfg, CancellationToken ct)
    {
        // Log what we would do – never log actual credentials to stdout.
        Console.WriteLine("[T4Connector] STUB: ConnectAsync called");
        Console.WriteLine($"[T4Connector] STUB: host={cfg.T4Host}:{cfg.T4Port}");
        Console.WriteLine($"[T4Connector] STUB: user={cfg.T4User} license=<redacted>");
        Console.WriteLine("[T4Connector] STUB: Real connectivity requires T4.Api NuGet package.");
        Console.WriteLine("[T4Connector] STUB: See docs/DotNet_Worker_and_IPC.md for next steps.");

        if (string.IsNullOrEmpty(cfg.T4User) || string.IsNullOrEmpty(cfg.T4Password))
        {
            Console.WriteLine("[T4Connector] STUB: No credentials configured – returning RC_NOT_CONNECTED.");
            return Task.FromResult("ERROR no credentials configured");
        }

        // With credentials present, report "connected" (stub) so integration
        // tests can assert the full flow end-to-end without the real SDK.
        return Task.FromResult("OK (stub) connected to " + cfg.T4Host);
    }

    public Task<string> PlaceOrderAsync(string fields, CancellationToken ct)
    {
        Console.WriteLine("[T4Connector] STUB: PlaceOrderAsync fields=" + fields);
        return Task.FromResult("OK (stub) order logged");
    }
}
