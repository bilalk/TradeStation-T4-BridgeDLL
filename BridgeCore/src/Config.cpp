#include "Config.h"
#include "Types.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>

namespace Bridge {

static std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n\"");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n\",");
    return s.substr(b, e - b + 1);
}

static std::string ToUpper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return r;
}

int LoadConfig(const std::string& path, BridgeConfig& out) noexcept {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return RC_CONFIG_ERR;

        out = DefaultConfig();

        std::string line;
        while (std::getline(f, line)) {
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string key = Trim(line.substr(0, colon));
            std::string val = Trim(line.substr(colon + 1));

            std::string ku = ToUpper(key);
            if      (ku == "ADAPTERTYPE")  out.adapterType   = ToUpper(val);
            else if (ku == "LOGFILEPATH")  out.logFilePath   = val;
            else if (ku == "LOGTOCONSOLE") out.logToConsole  = (ToUpper(val) == "TRUE");
        }
        return RC_SUCCESS;
    }
    catch (...) {
        return RC_CONFIG_ERR;
    }
}

BridgeConfig DefaultConfig() noexcept {
    BridgeConfig cfg;
    cfg.adapterType  = "MOCK";
    cfg.logFilePath  = "logs/bridge.log";
    cfg.logToConsole = false;
    return cfg;
}

} // namespace Bridge
