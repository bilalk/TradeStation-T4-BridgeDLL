using System.IO.Pipes;

namespace BridgeDotNetWorker;

/// <summary>
/// Named pipe server that handles one client connection at a time.
/// Protocol: line-oriented (UTF-8).  Each request is a single line; the
/// server responds with a single line.
///
/// Supported commands:
///   PING                  → PONG
///   CONNECT               → OK … | ERROR …
///   PLACE symbol=… …     → OK … | ERROR …
///   SHUTDOWN              → OK shutting down  (then exits the server loop)
/// </summary>
public class PipeServer
{
    private readonly string        _pipeName;
    private readonly WorkerConfig  _config;
    private readonly IT4Connector  _connector;

    public PipeServer(string pipeName, WorkerConfig config, IT4Connector connector)
    {
        // NamedPipeServerStream expects only the short name (e.g. "BridgeDotNetWorker"),
        // not the full \\.\pipe\ path.
        int idx = pipeName.IndexOf(@"\pipe\", StringComparison.OrdinalIgnoreCase);
        _pipeName  = idx >= 0 ? pipeName[(idx + 6)..] : pipeName;
        if (string.IsNullOrEmpty(_pipeName)) _pipeName = "BridgeDotNetWorker";
        _config    = config;
        _connector = connector;
    }

    /// <summary>Run until cancellation is requested or SHUTDOWN command received.</summary>
    public async Task RunAsync(CancellationToken ct)
    {
        Console.WriteLine($"[PipeServer] Listening on pipe: {_pipeName}");

        while (!ct.IsCancellationRequested)
        {
            using var pipe = new NamedPipeServerStream(
                _pipeName,
                PipeDirection.InOut,
                NamedPipeServerStream.MaxAllowedServerInstances,
                PipeTransmissionMode.Byte,
                PipeOptions.WriteThrough | PipeOptions.Asynchronous);

            try
            {
                await pipe.WaitForConnectionAsync(ct).ConfigureAwait(false);
                Console.WriteLine("[PipeServer] Client connected");
                bool shutdown = await ServeClientAsync(pipe, ct).ConfigureAwait(false);
                if (shutdown) break;
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex)
            {
                Console.WriteLine("[PipeServer] Connection error: " + ex.Message);
            }
        }

        Console.WriteLine("[PipeServer] Stopped.");
    }

    /// <summary>
    /// Serve one connected client.
    /// Returns true if the server should shut down.
    /// </summary>
    private async Task<bool> ServeClientAsync(NamedPipeServerStream pipe, CancellationToken ct)
    {
        using var reader = new StreamReader(pipe, leaveOpen: true);
        using var writer = new StreamWriter(pipe, leaveOpen: true) { AutoFlush = true, NewLine = "\n" };

        while (!ct.IsCancellationRequested && pipe.IsConnected)
        {
            string? line;
            try
            {
                line = await reader.ReadLineAsync(ct).ConfigureAwait(false);
            }
            catch
            {
                break;
            }

            if (line is null) break;
            line = line.Trim();
            if (line.Length == 0) continue;

            Console.WriteLine("[PipeServer] Received: " + line);

            string response;
            bool shutdown = false;

            if (line == "PING")
            {
                response = "PONG";
            }
            else if (line == "CONNECT")
            {
                response = await _connector.ConnectAsync(_config, ct).ConfigureAwait(false);
            }
            else if (line.StartsWith("PLACE", StringComparison.Ordinal))
            {
                string fields = line.Length > 5 ? line[6..] : "";
                response = await _connector.PlaceOrderAsync(fields, ct).ConfigureAwait(false);
            }
            else if (line == "SHUTDOWN")
            {
                response = "OK shutting down";
                shutdown = true;
            }
            else
            {
                response = "ERROR unknown command: " + line;
            }

            Console.WriteLine("[PipeServer] Sending: " + response);
            await writer.WriteLineAsync(response).ConfigureAwait(false);

            if (shutdown) return true;
        }

        return false;
    }
}
