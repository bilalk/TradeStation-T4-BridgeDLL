namespace BridgeDotNetWorker;

/// <summary>
/// Appends command/response pairs to <c>logs/bridge_responses.log</c>.
/// Rotates the file on startup if it already holds ≥ 1000 lines (keeps newest half).
/// </summary>
public static class ResponseLogger
{
    private const int MaxLines = 1000;

    private static readonly string _logPath;
    private static readonly object  _lock = new();

    static ResponseLogger()
    {
        string logsDir = ResolveLogsDir();
        Directory.CreateDirectory(logsDir);
        _logPath = Path.Combine(logsDir, "bridge_responses.log");
        RotateIfNeeded();
    }

    /// <summary>Appends one log line: <c>[timestamp] command -> result</c>.</summary>
    public static void Log(string command, string result)
    {
        string line = $"[{DateTime.UtcNow:yyyy-MM-ddTHH:mm:ssZ}] {command} -> {result}";
        lock (_lock)
        {
            try   { File.AppendAllText(_logPath, line + Environment.NewLine); }
            catch (Exception ex) { Console.Error.WriteLine($"[ResponseLogger] write failed: {ex.Message}"); }
        }
    }

    // ── helpers ────────────────────────────────────────────────────────────────

    private static string ResolveLogsDir()
    {
        // Walk up from the executable directory until we find the repo root
        // (identified by the presence of a "config" or "scripts" sub-directory).
        string? dir = AppContext.BaseDirectory;
        for (int depth = 0; depth < 8 && dir is not null; depth++)
        {
            if (Directory.Exists(Path.Combine(dir, "config")) ||
                Directory.Exists(Path.Combine(dir, "scripts")))
                return Path.Combine(dir, "logs");
            dir = Path.GetDirectoryName(dir);
        }
        // Fallback: logs/ beside the executable
        return Path.Combine(AppContext.BaseDirectory, "logs");
    }

    private static void RotateIfNeeded()
    {
        lock (_lock)
        {
            try
            {
                if (!File.Exists(_logPath)) return;
                string[] lines = File.ReadAllLines(_logPath);
                if (lines.Length >= MaxLines)
                    // Keep the newest half (last MaxLines/2 lines) to trim the file.
                    File.WriteAllLines(_logPath, lines[^(MaxLines / 2)..]); 
            }
            catch { /* best-effort */ }
        }
    }
}
