#include "Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctime>
#include <cstdio>

namespace Logger {
    static HANDLE       g_hFile = INVALID_HANDLE_VALUE;
    static CRITICAL_SECTION g_cs;
    static bool         g_csInit = false;

    void Init(const std::string& logPath) {
        if (!g_csInit) {
            InitializeCriticalSection(&g_cs);
            g_csInit = true;
        }
        if (g_hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hFile);
        }
        g_hFile = CreateFileA(
            logPath.c_str(),
            GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_hFile != INVALID_HANDLE_VALUE) {
            SetFilePointer(g_hFile, 0, nullptr, FILE_END);
        }
    }

    void Log(const std::string& msg) {
        // Always write to OutputDebugString for easy debugging.
        OutputDebugStringA(("[BridgeDLL] " + msg + "\n").c_str());

        if (!g_csInit) return;
        EnterCriticalSection(&g_cs);

        time_t t = time(nullptr);
        char ts[32];
        struct tm tmBuf{};
        gmtime_s(&tmBuf, &t);
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);

        std::string line = std::string(ts) + " " + msg + "\r\n";

        if (g_hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(g_hFile, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
            FlushFileBuffers(g_hFile);
        }
        LeaveCriticalSection(&g_cs);
    }

    void Shutdown() {
        if (g_hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hFile);
            g_hFile = INVALID_HANDLE_VALUE;
        }
        if (g_csInit) {
            DeleteCriticalSection(&g_cs);
            g_csInit = false;
        }
    }
}
