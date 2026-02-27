#include "Logger.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <chrono>
#include <ctime>
#include <filesystem>

namespace Bridge {

static std::mutex    g_logMutex;
static std::ofstream g_logFile;
static bool          g_logToConsole = false;

static std::string CurrentTimestamp() noexcept {
    try {
        auto now  = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[32] = {};
#ifdef _WIN32
        struct tm tm_info;
        localtime_s(&tm_info, &t);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
#else
        struct tm* tm_info = std::localtime(&t);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
#endif
        return buf;
    }
    catch (...) { return "0000-00-00 00:00:00"; }
}

static const char* LevelStr(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::DEBUG_:   return "DEBUG";
        case LogLevel::INFO:     return "INFO ";
        case LogLevel::WARNING_: return "WARN ";
        case LogLevel::ERROR_:   return "ERROR";
        default:                 return "?????";
    }
}

void LogInit(const std::string& filePath, bool logToConsole) noexcept {
    try {
        std::lock_guard<std::mutex> lk(g_logMutex);
        g_logToConsole = logToConsole;
        if (!filePath.empty()) {
            // Create parent directories if needed
            std::filesystem::path p(filePath);
            if (p.has_parent_path())
                std::filesystem::create_directories(p.parent_path());
            g_logFile.open(filePath, std::ios::app);
        }
    }
    catch (...) {}
}

void Log(LogLevel level, const std::string& message) noexcept {
    try {
        std::string line = "[" + CurrentTimestamp() + "] [" +
                           LevelStr(level) + "] " + message + "\n";
        std::lock_guard<std::mutex> lk(g_logMutex);
        if (g_logFile.is_open())
            g_logFile << line << std::flush;
        if (g_logToConsole)
            std::cout << line << std::flush;
    }
    catch (...) {}
}

} // namespace Bridge
