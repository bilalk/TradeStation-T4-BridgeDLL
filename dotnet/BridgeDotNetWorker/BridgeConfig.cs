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

    // ── Secrets: only from env vars, never persisted in JSON ─────────────────
    /// <summary>Set via T4_PASSWORD env var. Never stored in config file.</summary>
    public string? T4Password   { get; private set; }

    /// <summary>Set via T4_LICENSE_KEY env var. Never stored in config file.</summary>
    public string? T4LicenseKey { get; private set; }

    /// <summary>Set via T4_FIRM or BRIDGE_T4_FIRM env var. Defaults to empty string.</summary>
    public string? T4Firm       { get; private set; }

    // ── FIX 4.2 session identifiers ──────────────────────────────────────────
    /// <summary>FIX SenderCompID sent by this client. Defaults to T4Username when empty.</summary>
    public string FixSenderCompID { get; set; } = string.Empty;

    /// <summary>FIX TargetCompID of the T4 server. Default: CTS.</summary>
    public string FixTargetCompID { get; set; } = "CTS";

    /// <summary>FIX SenderSubID. Usually empty for T4.</summary>
    public string FixSenderSubID  { get; set; } = string.Empty;

    /// <summary>FIX TargetSubID of the T4 server. Default: T4FIX.</summary>
    public string FixTargetSubID  { get; set; } = "T4FIX";

    /// <summary>FIX HeartBtInt in seconds. Default: 30.</summary>
    public int    FixHeartBtInt   { get; set; } = 30;

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

                if (root.TryGetProperty("pipeName", out prop))
                    cfg.PipeName = prop.GetString() ?? cfg.PipeName;

                if (root.TryGetProperty("fixSenderCompId", out prop))
                    cfg.FixSenderCompID = prop.GetString() ?? cfg.FixSenderCompID;

                if (root.TryGetProperty("fixTargetCompId", out prop))
                    cfg.FixTargetCompID = prop.GetString() ?? cfg.FixTargetCompID;

                if (root.TryGetProperty("fixSenderSubId", out prop))
                    cfg.FixSenderSubID = prop.GetString() ?? cfg.FixSenderSubID;

                if (root.TryGetProperty("fixTargetSubId", out prop))
                    cfg.FixTargetSubID = prop.GetString() ?? cfg.FixTargetSubID;

                if (root.TryGetProperty("fixHeartBtInt", out prop))
                    cfg.FixHeartBtInt = prop.GetInt32();
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[BridgeConfig] Warning: could not parse {jsonPath}: {ex.Message}");
            }
        }

        // 2. Env-var overrides (always take precedence)
        cfg.Connector   = Env("BRIDGE_CONNECTOR")   ?? cfg.Connector;
        cfg.T4Host      = Env("T4_HOST")            ?? cfg.T4Host;
        cfg.T4Port      = int.TryParse(Env("T4_PORT"), out int p) ? p : cfg.T4Port;
        cfg.T4Username  = Env("T4_USERNAME")        ?? cfg.T4Username;
        cfg.PipeName    = Env("BRIDGE_PIPE_NAME")   ?? cfg.PipeName;

        // FIX session identifier overrides
        cfg.FixSenderCompID = Env("FIX_SENDER_COMP_ID") ?? cfg.FixSenderCompID;
        cfg.FixTargetCompID = Env("FIX_TARGET_COMP_ID") ?? cfg.FixTargetCompID;
        cfg.FixSenderSubID  = Env("FIX_SENDER_SUB_ID")  ?? cfg.FixSenderSubID;
        cfg.FixTargetSubID  = Env("FIX_TARGET_SUB_ID")  ?? cfg.FixTargetSubID;
        cfg.FixHeartBtInt   = int.TryParse(Env("FIX_HEART_BT_INT"), out int hb) ? hb : cfg.FixHeartBtInt;

        // 3. Secrets only from env vars
        cfg.T4Password   = Env("T4_PASSWORD");
        cfg.T4LicenseKey = Env("T4_LICENSE_KEY");
        cfg.T4Firm       = Env("T4_FIRM") ?? Env("BRIDGE_T4_FIRM");

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
