#include "IAdapter.h"
#include "Config.h"
#include "Logger.h"
#include "ResultCodes.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// IPC helpers
// ---------------------------------------------------------------------------
// Timeout for pipe operations (ms).
static constexpr DWORD PIPE_TIMEOUT_MS = 5000;

// Send one line to the pipe and read back one line response.
// Returns the response string, or throws on error.
static std::string PipeCall(HANDLE hPipe, const std::string& request) {
    // Write request line
    std::string line = request + "\n";
    DWORD written = 0;
    if (!WriteFile(hPipe, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr)) {
        throw std::runtime_error("WriteFile failed: " + std::to_string(GetLastError()));
    }

    // Read response line (up to 4 KB)
    char buf[4096] = {};
    DWORD read = 0;
    if (!ReadFile(hPipe, buf, sizeof(buf) - 1, &read, nullptr)) {
        throw std::runtime_error("ReadFile failed: " + std::to_string(GetLastError()));
    }
    std::string resp(buf, read);
    // Strip trailing \r\n
    while (!resp.empty() && (resp.back() == '\r' || resp.back() == '\n')) resp.pop_back();
    return resp;
}

// ---------------------------------------------------------------------------
// DotNetAdapter
// ---------------------------------------------------------------------------
class DotNetAdapter : public IAdapter {
public:
    explicit DotNetAdapter(const BridgeConfig& cfg)
        : m_cfg(cfg), m_hProcess(nullptr), m_hPipe(INVALID_HANDLE_VALUE),
          m_connected(false) {}

    ~DotNetAdapter() override {
        Disconnect();
    }

    // Ensure worker is running and pipe is open. Called lazily.
    // Returns false if unable to establish connection.
    bool EnsureConnected() {
        if (m_connected && m_hPipe != INVALID_HANDLE_VALUE) return true;

        // Spawn worker process if not already running.
        if (!SpawnWorker()) return false;

        // Wait for the pipe server to come up (worker needs ~500ms to start).
        if (!ConnectPipe()) return false;

        // Verify with PING.
        try {
            std::string resp = PipeCall(m_hPipe, "PING");
            if (resp != "PONG") {
                Logger::Log("[DotNetAdapter] Unexpected PING response: " + resp);
                return false;
            }
            Logger::Log("[DotNetAdapter] PING/PONG OK");
            m_connected = true;
        } catch (const std::exception& ex) {
            Logger::Log(std::string("[DotNetAdapter] PING failed: ") + ex.what());
            return false;
        }
        return true;
    }

    int Execute(const OrderRequest& req) override {
        if (!EnsureConnected()) {
            Logger::Log("[DotNetAdapter] Execute: not connected");
            return RC_NOT_CONNECTED;
        }

        try {
            std::string response;
            switch (req.cmd) {
                case OrderCmd::PLACE: {
                    std::ostringstream oss;
                    oss << "PLACE"
                        << " symbol=" << req.symbol
                        << " account=" << req.account
                        << " qty=" << req.qty
                        << " price=" << req.price
                        << " side=" << req.side
                        << " type=" << req.type;
                    response = PipeCall(m_hPipe, oss.str());
                    Logger::Log("[DotNetAdapter] PLACE response: " + response);
                    // Accept "OK" or any response starting with "OK"
                    return (response.rfind("OK", 0) == 0) ? RC_SUCCESS : RC_ERROR;
                }
                case OrderCmd::CANCEL:
                case OrderCmd::MODIFY:
                    Logger::Log("[DotNetAdapter] CANCEL/MODIFY not yet implemented; returning RC_SUCCESS");
                    return RC_SUCCESS;
            }
        } catch (const std::exception& ex) {
            Logger::Log(std::string("[DotNetAdapter] Execute exception: ") + ex.what());
            // Invalidate connection so next call re-establishes.
            m_connected = false;
            if (m_hPipe != INVALID_HANDLE_VALUE) {
                CloseHandle(m_hPipe);
                m_hPipe = INVALID_HANDLE_VALUE;
            }
            return RC_ERROR;
        }
        return RC_ERROR;
    }

    bool IsConnected() const override { return m_connected; }

private:
    BridgeConfig m_cfg;
    HANDLE       m_hProcess;
    HANDLE       m_hPipe;
    bool         m_connected;

    bool SpawnWorker() {
        if (m_hProcess) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                return true; // still running
            }
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }

        // Determine worker exe path.
        std::string workerPath = m_cfg.dotnetWorkerPath;
if (workerPath.empty()) {
    // Default: same directory as the module containing this code, then into dotnet sub-folder.
    std::string dir = GetModuleDirectoryFromAddress(reinterpret_cast<const void*>(&GetModuleDirectoryFromAddress));
    if (dir.empty()) {
        Logger::Log("[DotNetAdapter] Failed to determine module directory; cannot locate worker.");
        return false;
    }
    workerPath = dir + "dotnet\\BridgeDotNetWorker\\BridgeDotNetWorker.exe";
}
        // Convert pipe name to command-line arg.
        std::string cmdLine = "\"" + workerPath + "\" --pipe \"" + m_cfg.pipeName + "\"";

        Logger::Log("[DotNetAdapter] Spawning worker: " + cmdLine);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput  = INVALID_HANDLE_VALUE;
        si.hStdOutput = INVALID_HANDLE_VALUE;
        si.hStdError  = INVALID_HANDLE_VALUE;

        // Convert to wide strings for CreateProcessW
        int wlen = MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, nullptr, 0);
        std::wstring wCmd(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, wCmd.data(), wlen);

        if (!CreateProcessW(nullptr, wCmd.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            Logger::Log("[DotNetAdapter] CreateProcessW failed: " + std::to_string(GetLastError()));
            return false;
        }

        CloseHandle(pi.hThread);
        m_hProcess = pi.hProcess;
        Logger::Log("[DotNetAdapter] Worker PID: " + std::to_string(pi.dwProcessId));
        return true;
    }

    bool ConnectPipe() {
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }

        // Convert pipe name to wide
        std::wstring wPipe(m_cfg.pipeName.begin(), m_cfg.pipeName.end());

        // Retry for up to PIPE_TIMEOUT_MS with 100ms intervals.
        DWORD deadline = GetTickCount() + PIPE_TIMEOUT_MS;
        while (GetTickCount() < deadline) {
            m_hPipe = CreateFileW(
                wPipe.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING,
                0, nullptr);

            if (m_hPipe != INVALID_HANDLE_VALUE) {
                DWORD mode = PIPE_READMODE_BYTE;
                SetNamedPipeHandleState(m_hPipe, &mode, nullptr, nullptr);
                Logger::Log("[DotNetAdapter] Connected to pipe: " + m_cfg.pipeName);
                return true;
            }

            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PIPE_BUSY) {
                Sleep(100);
                continue;
            }
            Logger::Log("[DotNetAdapter] ConnectPipe error: " + std::to_string(err));
            return false;
        }
        Logger::Log("[DotNetAdapter] Timed out waiting for pipe: " + m_cfg.pipeName);
        return false;
    }

    void Disconnect() {
        m_connected = false;
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 0);
            WaitForSingleObject(m_hProcess, 1000);
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }
    }
};

IAdapter* CreateDotNetAdapter(const BridgeConfig& cfg) {
    return new DotNetAdapter(cfg);
}
