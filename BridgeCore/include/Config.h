#pragma once
#include <string>

namespace Bridge {

struct BridgeConfig {
    std::string adapterType;   // "MOCK", "FIX", "DOTNET"
    std::string logFilePath;   // path to log file; default "logs/bridge.log"
    bool        logToConsole = false;
};

// Load config from the given JSON file path.
// Returns RC_SUCCESS or RC_CONFIG_ERR.
int LoadConfig(const std::string& path, BridgeConfig& out) noexcept;

// Return a default config (MOCK adapter, logs/bridge.log).
BridgeConfig DefaultConfig() noexcept;

} // namespace Bridge
