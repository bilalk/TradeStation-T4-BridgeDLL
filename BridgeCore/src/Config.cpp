#include "Config.h"
#include "Logger.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Minimal JSON-value extractor for a flat object.
// Parses lines of the form:  "key": "value"  or  "key": number
// This keeps us dependency-free while being easy to extend.
// ---------------------------------------------------------------------------
static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    // Search for  "key":  then capture up to closing "
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    // skip whitespace and colon
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return {};
    if (json[pos] == '"') {
        ++pos;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) { ++pos; }
            val += json[pos++];
        }
        return val;
    }
    // number or bare value – read until comma/newline/}
    std::string val;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '\n' && json[pos] != '\r' && json[pos] != '}') {
        val += json[pos++];
    }
    // trim trailing whitespace
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    return val;
}

bool BridgeConfig::LoadFromFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string json = oss.str();

    auto get = [&](const std::string& k) { return ExtractJsonString(json, k); };

    auto s = get("adapterType");      if (!s.empty()) adapterType      = s;
    s = get("dotnetWorkerPath");      if (!s.empty()) dotnetWorkerPath  = s;
    s = get("pipeName");              if (!s.empty()) pipeName          = s;
    s = get("t4Host");                if (!s.empty()) t4Host            = s;
    s = get("t4Port");                if (!s.empty()) { try { t4Port = std::stoi(s); } catch (...) { Logger::Log("Config: invalid t4Port value: " + s); } }
    s = get("t4User");                if (!s.empty()) t4User            = s;
    s = get("t4Password");            if (!s.empty()) t4Password        = s;
    s = get("t4License");             if (!s.empty()) t4License         = s;
    s = get("logPath");               if (!s.empty()) logPath           = s;

    Logger::Log("Config loaded from: " + path);
    return true;
}

void BridgeConfig::ApplyEnvOverrides() {
    auto env = [](const char* name) -> std::string {
        char* v = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&v, &sz, name) == 0 && v) {
            std::string s(v);
            free(v);
            return s;
        }
        return {};
    };

    auto s = env("BRIDGE_ADAPTER_TYPE");      if (!s.empty()) adapterType      = s;
    s = env("BRIDGE_DOTNET_WORKER_PATH");     if (!s.empty()) dotnetWorkerPath  = s;
    s = env("BRIDGE_PIPE_NAME");              if (!s.empty()) pipeName          = s;
    s = env("BRIDGE_T4_HOST");               if (!s.empty()) t4Host            = s;
    s = env("BRIDGE_T4_PORT");               if (!s.empty()) { try { t4Port = std::stoi(s); } catch (...) { Logger::Log("Config: invalid BRIDGE_T4_PORT value: " + s); } }
    s = env("BRIDGE_T4_USER");               if (!s.empty()) t4User            = s;
    s = env("BRIDGE_T4_PASSWORD");           if (!s.empty()) t4Password        = s;
    s = env("BRIDGE_T4_LICENSE");            if (!s.empty()) t4License         = s;
    s = env("BRIDGE_LOG_PATH");              if (!s.empty()) logPath           = s;
}
