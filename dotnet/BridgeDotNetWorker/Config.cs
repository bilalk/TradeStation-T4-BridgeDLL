using System.Text.Json;

namespace BridgeDotNetWorker;

/// <summary>
/// Configuration read from config/bridge.json (relative to the repo root or
/// the executable) and optionally overridden by environment variables.
/// </summary>
public class WorkerConfig
{
    public string AdapterType      { get; set; } = "DOTNET";
    public string PipeName         { get; set; } = @"\\.\pipe\BridgeDotNetWorker";
    public string T4Host           { get; set; } = "uhfix-sim.t4login.com";
    public int    T4Port           { get; set; } = 10443;
    public string T4User           { get; set; } = "";
    public string T4Password       { get; set; } = "";
    public string T4License        { get; set; } = "";
    public string LogPath          { get; set; } = "bridge-worker.log";

    /// <summary>
    /// Load config from JSON file.  Keys are camelCase matching bridge.json schema.
    /// </summary>
    public static WorkerConfig Load(string? configPath = null)
    {
        var cfg = new WorkerConfig();

        // Locate config file
        string[] candidates = configPath is not null
            ? new[] { configPath }
            : new[]
            {
                Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "config", "bridge.json"),
                Path.Combine(AppContext.BaseDirectory, "config", "bridge.json"),
                Path.Combine(Directory.GetCurrentDirectory(), "config", "bridge.json"),
                "config/bridge.json",
                "bridge.json",
            };

        foreach (var candidate in candidates)
        {
            if (!File.Exists(candidate)) continue;
            try
            {
                var json = File.ReadAllText(candidate);
                var doc  = JsonDocument.Parse(json);
                var root = doc.RootElement;

                if (root.TryGetProperty("adapterType",  out var v)) cfg.AdapterType  = v.GetString() ?? cfg.AdapterType;
                if (root.TryGetProperty("pipeName",     out v))     cfg.PipeName     = v.GetString() ?? cfg.PipeName;
                if (root.TryGetProperty("t4Host",       out v))     cfg.T4Host       = v.GetString() ?? cfg.T4Host;
                if (root.TryGetProperty("t4Port",       out v))     cfg.T4Port       = v.GetInt32();
                if (root.TryGetProperty("t4User",       out v))     cfg.T4User       = v.GetString() ?? cfg.T4User;
                if (root.TryGetProperty("t4Password",   out v))     cfg.T4Password   = v.GetString() ?? cfg.T4Password;
                if (root.TryGetProperty("t4License",    out v))     cfg.T4License    = v.GetString() ?? cfg.T4License;
                if (root.TryGetProperty("logPath",      out v))     cfg.LogPath      = v.GetString() ?? cfg.LogPath;

                Console.WriteLine($"[Config] Loaded from: {Path.GetFullPath(candidate)}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Config] Failed to parse {candidate}: {ex.Message}");
            }
            break;
        }

        // Environment variable overrides (used in GitHub Actions / CI)
        cfg.AdapterType  = Env("BRIDGE_ADAPTER_TYPE",   cfg.AdapterType);
        cfg.PipeName     = Env("BRIDGE_PIPE_NAME",      cfg.PipeName);
        cfg.T4Host       = Env("BRIDGE_T4_HOST",        cfg.T4Host);
        if (int.TryParse(Env("BRIDGE_T4_PORT", ""), out int port)) cfg.T4Port = port;
        cfg.T4User       = Env("BRIDGE_T4_USER",        cfg.T4User);
        cfg.T4Password   = Env("BRIDGE_T4_PASSWORD",    cfg.T4Password);
        cfg.T4License    = Env("BRIDGE_T4_LICENSE",     cfg.T4License);
        cfg.LogPath      = Env("BRIDGE_LOG_PATH",       cfg.LogPath);

        return cfg;
    }

    private static string Env(string name, string fallback)
        => Environment.GetEnvironmentVariable(name) is { Length: > 0 } v ? v : fallback;
}
