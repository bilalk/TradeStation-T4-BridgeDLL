using BridgeDotNetWorker;

// ---------------------------------------------------------------------------
// BridgeDotNetWorker – entry point
//
// Usage:
//   BridgeDotNetWorker.exe [--pipe <pipeName>] [--config <path>]
//
// Default pipe name: \\.\pipe\BridgeDotNetWorker
// ---------------------------------------------------------------------------

string? pipeArg   = null;
string? configArg = null;

for (int i = 0; i < args.Length - 1; i++)
{
    if (args[i] == "--pipe")   pipeArg   = args[i + 1];
    if (args[i] == "--config") configArg = args[i + 1];
}

Console.WriteLine("[Worker] BridgeDotNetWorker starting");
Console.WriteLine($"[Worker] Args: {string.Join(" ", args)}");

var config    = WorkerConfig.Load(configArg);
var connector = new StubT4Connector();

// Use pipe name from command-line if provided (overrides config).
string pipeName = pipeArg ?? config.PipeName;

// Strip the \\.\pipe\ prefix that Windows requires in Win32 but .NET
// NamedPipeServerStream takes only the short name (e.g. "BridgeDotNetWorker").
string shortPipeName = pipeName;
int pipeIdx = pipeName.IndexOf(@"\pipe\", StringComparison.OrdinalIgnoreCase);
if (pipeIdx >= 0)
    shortPipeName = pipeName[(pipeIdx + 6)..];
if (string.IsNullOrEmpty(shortPipeName))
    shortPipeName = "BridgeDotNetWorker";

Console.WriteLine($"[Worker] Pipe name (short): {shortPipeName}");
Console.WriteLine($"[Worker] Adapter type: {config.AdapterType}");

using var cts = new CancellationTokenSource();
Console.CancelKeyPress += (_, e) => { e.Cancel = true; cts.Cancel(); };

var server = new PipeServer(shortPipeName, config, connector);
await server.RunAsync(cts.Token);

Console.WriteLine("[Worker] Exiting.");
