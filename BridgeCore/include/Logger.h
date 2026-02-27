#pragma once
#include <string>

// Simple synchronous file logger.
// All methods are thread-safe via a critical section.
namespace Logger {
    void Init(const std::string& logPath);
    void Log(const std::string& msg);
    void Shutdown();
}
