using System.IO.Pipes;
using System.Text;

namespace BridgeDotNetWorker;

/// <summary>
/// Named-pipe IPC server.  Accepts line-delimited text commands from a local client:
/// <list type="bullet">
///   <item><term>PING</term><description>Returns "OK PONG"</description></item>
///   <item><term>CONNECT</term><description>Connects to T4 using loaded config. Returns "OK ..." or "ERROR ..."</description></item>
///   <item><term>PLACE symbol side qty price [type]</term><description>Places an order. Returns "OK ..." or "ERROR ..."</description></item>
///   <item><term>EXIT</term><description>Shuts down the server.</description></item>
/// </list>
/// </summary>
public sealed class PipeServer : IDisposable
{
    private readonly BridgeConfig _cfg;
    private readonly IT4Connector _connector;
    private readonly CancellationTokenSource _cts = new();
    private bool _disposed;

    public PipeServer(BridgeConfig cfg, IT4Connector connector)
    {
        _cfg       = cfg;
        _connector = connector;
    }

    /// <summary>Runs the pipe server until an EXIT command is received or the token is cancelled.</summary>
    public async Task RunAsync(CancellationToken externalToken = default)
    {
        using var linked = CancellationTokenSource.CreateLinkedTokenSource(externalToken, _cts.Token);
        CancellationToken token = linked.Token;

        Console.WriteLine($"[PipeServer] Listening on pipe '{_cfg.PipeName}' …");

        while (!token.IsCancellationRequested)
        {
            await using var pipe = new NamedPipeServerStream(
                _cfg.PipeName,
                PipeDirection.InOut,
                maxNumberOfServerInstances: 1,
                transmissionMode: PipeTransmissionMode.Byte,
                options: PipeOptions.Asynchronous);

            try
            {
                await pipe.WaitForConnectionAsync(token);
                Console.WriteLine("[PipeServer] Client connected.");
                await HandleClientAsync(pipe, token);
                Console.WriteLine("[PipeServer] Client disconnected.");
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex) when (!token.IsCancellationRequested)
            {
                Console.Error.WriteLine($"[PipeServer] Error: {ex.Message}");
            }
        }

        Console.WriteLine("[PipeServer] Shutting down.");
    }

    private async Task HandleClientAsync(NamedPipeServerStream pipe, CancellationToken token)
    {
        using var reader = new StreamReader(pipe, Encoding.UTF8, leaveOpen: true);
        using var writer = new StreamWriter(pipe, Encoding.UTF8, leaveOpen: true) { AutoFlush = true };

        while (!token.IsCancellationRequested && pipe.IsConnected)
        {
            string? line = await reader.ReadLineAsync(token);
            if (line is null) break;  // client disconnected

            string response = ProcessCommand(line.Trim());
            await writer.WriteLineAsync(response.AsMemory(), token);

            if (line.Trim().Equals("EXIT", StringComparison.OrdinalIgnoreCase))
            {
                _cts.Cancel();
                break;
            }
        }
    }

    private string ProcessCommand(string command)
    {
        if (string.IsNullOrEmpty(command))
            return "ERROR empty command";

        string[] parts = command.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        string verb = parts[0].ToUpperInvariant();

        try
        {
            return verb switch
            {
                "PING"    => _connector.Ping(),
                "CONNECT" => _connector.Connect(_cfg),
                "PLACE"   => HandlePlace(parts),
                "EXIT"    => "OK bye",
                _         => $"ERROR unknown command '{verb}'"
            };
        }
        catch (Exception ex)
        {
            return $"ERROR {ex.Message}";
        }
    }

    private string HandlePlace(string[] parts)
    {
        // PLACE <symbol> <side> <qty> <price> [type]
        if (parts.Length < 5)
            return "ERROR usage: PLACE <symbol> <side> <qty> <price> [type]";

        string symbol    = parts[1];
        string side      = parts[2];
        if (!int.TryParse(parts[3], out int qty))
            return "ERROR qty must be an integer";
        if (!decimal.TryParse(parts[4], System.Globalization.NumberStyles.Any,
                System.Globalization.CultureInfo.InvariantCulture, out decimal price))
            return "ERROR price must be a decimal number";
        string orderType = parts.Length >= 6 ? parts[5] : "LIMIT";

        return _connector.PlaceOrder(symbol, side, qty, price, orderType);
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            _cts.Cancel();
            _cts.Dispose();
            _disposed = true;
        }
    }
}
