using BridgeDotNetWorker;

// ── Parse command-line args ──────────────────────────────────────────────────
string? configPath = null;
string? pipeName   = null;

for (int i = 0; i < args.Length; i++)
{
    if ((args[i] == "--config" || args[i] == "-c") && i + 1 < args.Length)
        configPath = args[++i];
    else if ((args[i] == "--pipe" || args[i] == "-p") && i + 1 < args.Length)
        pipeName = args[++i];
}

// ── Load config ───────────────────────────────────────────────────────────────
BridgeConfig cfg = BridgeConfig.Load(configPath);
if (pipeName is not null)
    cfg.PipeName = pipeName;

Console.WriteLine($"[Worker] Connector type : {cfg.Connector}");
Console.WriteLine($"[Worker] Pipe name      : {cfg.PipeName}");
Console.WriteLine($"[Worker] T4 host        : {cfg.T4Host}:{cfg.T4Port}");

// ── Create connector ──────────────────────────────────────────────────────────
IT4Connector connector = ConnectorFactory.Create(cfg);
Console.WriteLine($"[Worker] Using connector: {connector.GetType().Name}");

// ── Start pipe server ─────────────────────────────────────────────────────────
using var cts    = new CancellationTokenSource();
using var server = new PipeServer(cfg, connector);

Console.CancelKeyPress += (_, e) =>
{
    e.Cancel = true;
    cts.Cancel();
};

await server.RunAsync(cts.Token);
