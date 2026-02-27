#pragma once
#include <string>

// Configuration loaded from config/bridge.json (or environment variables).
struct BridgeConfig {
    // Which adapter back-end to use: "MOCK", "FIX", or "DOTNET"
    std::string adapterType     = "MOCK";

    // Path to the .NET worker exe (relative or absolute).
    // Defaults to "dotnet\\BridgeDotNetWorker\\BridgeDotNetWorker.exe" next to the DLL.
    std::string dotnetWorkerPath;

    // Named pipe name used for IPC with the .NET worker.
    std::string pipeName        = "\\\\.\\pipe\\BridgeDotNetWorker";

    // T4 / FIX endpoint
    std::string t4Host          = "uhfix-sim.t4login.com";
    int         t4Port          = 10443;

    // T4 credentials (never committed – loaded from config/bridge.json or env vars)
    std::string t4User;
    std::string t4Password;
    std::string t4License;

    // Log file path
    std::string logPath         = "bridge.log";

    // Load from a key=value or JSON-ish file at the given path.
    // Returns true on success.
    bool LoadFromFile(const std::string& path);

    // Override individual keys from environment variables:
    //   BRIDGE_ADAPTER_TYPE, BRIDGE_T4_HOST, BRIDGE_T4_PORT,
    //   BRIDGE_T4_USER, BRIDGE_T4_PASSWORD, BRIDGE_T4_LICENSE,
    //   BRIDGE_PIPE_NAME, BRIDGE_DOTNET_WORKER_PATH, BRIDGE_LOG_PATH
    void ApplyEnvOverrides();
};
