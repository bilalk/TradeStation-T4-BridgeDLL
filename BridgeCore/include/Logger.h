#pragma once
#include <string>

namespace Bridge {

enum class LogLevel { DEBUG_, INFO, WARNING_, ERROR_ };

void LogInit(const std::string& filePath, bool logToConsole = false) noexcept;
void Log(LogLevel level, const std::string& message) noexcept;

inline void LogInfo   (const std::string& msg) noexcept { Log(LogLevel::INFO,     msg); }
inline void LogWarning(const std::string& msg) noexcept { Log(LogLevel::WARNING_, msg); }
inline void LogError  (const std::string& msg) noexcept { Log(LogLevel::ERROR_,   msg); }
inline void LogDebug  (const std::string& msg) noexcept { Log(LogLevel::DEBUG_,   msg); }

} // namespace Bridge
