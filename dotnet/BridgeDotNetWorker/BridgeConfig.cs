using System.Text.Json;

namespace BridgeDotNetWorker;

/// <summary>
/// Bridge configuration. Loaded from config/bridge.json (optional) with env-var overrides.
/// Secrets (T4Password, T4LicenseKey) are never read from JSON – only from environment variables.
/// </summary>
public sealed class BridgeConfig
{
    // ── Connector selection ──────────────────────────────────────────────────
    /// <summary>"STUB" (default) or "REAL".</summary>
    public string Connector { get; set; } = "STUB";

    // ── T4 simulator endpoint ────────────────────────────────────────────────
    public string T4Host     { get; set; } = "uhfix-sim.t4login.com";
    public int    T4Port     { get; set; } = 10443;
    public string T4Username { get; set; } = string.Empty;
    public string T4Firm     { get; set; } = string.Empty;

    // ── Secrets: only from env vars, never persisted in JSON ─────────────────
    /// <summary>Set via T4_PASSWORD env var. Never stored in config file.</summary>
    public string? T4Password   { get; private set; }

    /// <summary>Set via T4_LICENSE_KEY env var. Never stored in config file.</summary>
    public string? T4LicenseKey { get; private set; }

    // ── IPC ──────────────────────────────────────────────────────────────────
    public string PipeName { get; set; } = "BridgeT4Pipe";

    // ─────────────────────────────────────────────────────────────────────────

    /// <summary>
    /// Loads configuration from an optional JSON file, then applies env-var overrides.
    /// Secrets are only ever sourced from environment variables.
    /// </summary>
    public static BridgeConfig Load(string? configPath = null)
    {
        var cfg = new BridgeConfig();

        // 1. Read JSON config file
        string? jsonPath = configPath
            ?? FindDefaultConfigPath();

        if (jsonPath is not null && File.Exists(jsonPath))
        {
            try
            {
                string json = File.ReadAllText(jsonPath);
                using JsonDocument doc = JsonDocument.Parse(json);
                JsonElement root = doc.RootElement;

                if (root.TryGetProperty("connector", out JsonElement prop))
                    cfg.Connector = prop.GetString() ?? cfg.Connector;

                if (root.TryGetProperty("t4Host", out prop))
                    cfg.T4Host = prop.GetString() ?? cfg.T4Host;

                if (root.TryGetProperty("t4Port", out prop))
                    cfg.T4Port = prop.GetInt32();

                if (root.TryGetProperty("t4Username", out prop))
                    cfg.T4Username = prop.GetString() ?? cfg.T4Username;

                if (root.TryGetProperty("t4Firm", out prop))
                    cfg.T4Firm = prop.GetString() ?? cfg.T4Firm;

                if (root.TryGetProperty("pipeName", out prop))
                    cfg.PipeName = prop.GetString() ?? cfg.PipeName;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[BridgeConfig] Warning: could not parse {jsonPath}: {ex.Message}");
            }
        }

        // 2. Env-var overrides (always take precedence; BRIDGE_T4_* are fallbacks for T4_*)
        cfg.Connector   = Env("BRIDGE_CONNECTOR")                        ?? cfg.Connector;
        cfg.T4Host      = Env("T4_HOST")    ?? Env("BRIDGE_T4_HOST")     ?? cfg.T4Host;
        cfg.T4Port      = int.TryParse(Env("T4_PORT") ?? Env("BRIDGE_T4_PORT"), out int p) ? p : cfg.T4Port;
        cfg.T4Username  = Env("T4_USERNAME") ?? Env("BRIDGE_T4_USER")    ?? cfg.T4Username;
        cfg.T4Firm      = Env("T4_FIRM")    ?? Env("BRIDGE_T4_FIRM")     ?? cfg.T4Firm;
        cfg.PipeName    = Env("BRIDGE_PIPE_NAME")                        ?? cfg.PipeName;

        // 3. Secrets only from env vars
        cfg.T4Password   = Env("T4_PASSWORD")   ?? Env("BRIDGE_T4_PASSWORD");
        cfg.T4LicenseKey = Env("T4_LICENSE_KEY") ?? Env("BRIDGE_T4_LICENSE");

        return cfg;
    }

    private static string? FindDefaultConfigPath()
    {
        // Walk up from the executable to find config/bridge.json
        string? dir = AppContext.BaseDirectory;
        for (int depth = 0; depth < 6 && dir is not null; depth++)
        {
            string candidate = Path.Combine(dir, "config", "bridge.json");
            if (File.Exists(candidate))
                return candidate;
            dir = Path.GetDirectoryName(dir);
        }
        return null;
    }

    private static string? Env(string name) =>
        Environment.GetEnvironmentVariable(name) is { Length: > 0 } v ? v : null;
}
