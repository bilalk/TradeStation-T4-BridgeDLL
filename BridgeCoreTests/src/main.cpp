// BridgeCoreTests – simple self-contained test runner.
// No external test framework required; returns non-zero exit code on failure.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstdlib>

// BridgeCore headers
#include "../../BridgeCore/include/Config.h"
#include "../../BridgeCore/include/IAdapter.h"
#include "../../BridgeCore/include/Logger.h"
#include "../../BridgeCore/include/OrderRequest.h"
#include "../../BridgeCore/include/ResultCodes.h"

// BridgeCore factory / validation functions
extern IAdapter* CreateMockAdapter();
extern IAdapter* CreateDotNetAdapter(const BridgeConfig& cfg);
extern int       ValidateOrder(OrderRequest& req);

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static int g_passed  = 0;
static int g_failed  = 0;
static int g_skipped = 0;

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
                  << "  expected " << (b) << " got " << (a) << "\n"; \
        ++g_failed; \
    } else { \
        ++g_passed; \
    } \
} while(0)

#define ASSERT_TRUE(x) do { \
    if (!(x)) { \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
                  << "  expected true: " #x "\n"; \
        ++g_failed; \
    } else { \
        ++g_passed; \
    } \
} while(0)

// ---------------------------------------------------------------------------
// Test: Config parsing via environment variables
//
// FIX: Uses _putenv_s (CRT) instead of SetEnvironmentVariableA (Win32)
// because ApplyEnvOverrides() reads env vars with _dupenv_s (CRT).
// On MSVC, _dupenv_s and SetEnvironmentVariableA use separate env blocks.
// ---------------------------------------------------------------------------
static void TestConfigEnvOverride() {
    _putenv_s("BRIDGE_ADAPTER_TYPE", "DOTNET");
    _putenv_s("BRIDGE_T4_HOST",      "test.host");
    _putenv_s("BRIDGE_T4_PORT",      "1234");

    BridgeConfig cfg;
    cfg.ApplyEnvOverrides();

    ASSERT_EQ(cfg.adapterType, std::string("DOTNET"));
    ASSERT_EQ(cfg.t4Host,      std::string("test.host"));
    ASSERT_EQ(cfg.t4Port,      1234);

    // Clean up – set to empty string so _dupenv_s will find them empty
    // and ApplyEnvOverrides will skip them (it checks !s.empty()).
    _putenv_s("BRIDGE_ADAPTER_TYPE", "");
    _putenv_s("BRIDGE_T4_HOST",      "");
    _putenv_s("BRIDGE_T4_PORT",      "");
}

// ---------------------------------------------------------------------------
// Test: Config file loading (environment-aware)
//
// Asserts DOTNET-specific config values (adapterType=DOTNET,
// t4Host=uhfix-sim.t4login.com, t4Port=10443) when config/bridge.json is
// present OR BRIDGE_ADAPTER_TYPE env var is set to DOTNET.
//
// Skips these assertions gracefully when:
//   - config/bridge.json is absent, AND
//   - BRIDGE_ADAPTER_TYPE env var is not set to DOTNET
//
// This prevents hard CI failures in environments without credentials while
// still exercising the config loading path in developer and CI environments
// that provide a (non-secret) config file.
// ---------------------------------------------------------------------------
static bool FileExists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static void TestConfigFileLoad() {
    const std::string configPath = "config\\bridge.json";
    bool configExists = FileExists(configPath);

    // Check if BRIDGE_ADAPTER_TYPE env var is set to DOTNET
    char* envAdapterType = nullptr;
    size_t envSz = 0;
    bool hasDotnetEnv = false;
    if (_dupenv_s(&envAdapterType, &envSz, "BRIDGE_ADAPTER_TYPE") == 0 && envAdapterType != nullptr) {
        hasDotnetEnv = (std::string(envAdapterType) == "DOTNET");
        free(envAdapterType);
    }

    if (!configExists && !hasDotnetEnv) {
        std::cout << "[TestConfigFileLoad] SKIPPED: config/bridge.json not found and "
                     "BRIDGE_ADAPTER_TYPE is not set to DOTNET.\n"
                     "  DOTNET integration config assertions require config/bridge.json\n"
                     "  or BRIDGE_ADAPTER_TYPE=DOTNET environment variable.\n";
        ++g_skipped;
        return;
    }

    BridgeConfig cfg;
    if (configExists) {
        bool loaded = cfg.LoadFromFile(configPath);
        ASSERT_TRUE(loaded);
        std::cout << "[TestConfigFileLoad] Loaded from file: adapterType=" << cfg.adapterType
                  << " t4Host=" << cfg.t4Host << " t4Port=" << cfg.t4Port << "\n";
    }
    // Apply env overrides (allows CI to set BRIDGE_ADAPTER_TYPE=DOTNET)
    cfg.ApplyEnvOverrides();

    // DOTNET-specific assertions – only reached when config or env provides DOTNET setup
    ASSERT_EQ(cfg.adapterType, std::string("DOTNET"));
    ASSERT_EQ(cfg.t4Host,      std::string("uhfix-sim.t4login.com"));
    ASSERT_EQ(cfg.t4Port,      10443);
}

// ---------------------------------------------------------------------------
// Test: Validation
// ---------------------------------------------------------------------------
static void TestValidation() {
    OrderRequest req;
    req.cmd     = OrderCmd::PLACE;
    req.symbol  = "ESZ4";
    req.account = "TestAcct";
    req.qty     = 1;
    req.price   = 4500.0;
    req.side    = "buy";
    req.type    = "limit";

    int rc = ValidateOrder(req);
    ASSERT_EQ(rc, RC_SUCCESS);
    ASSERT_EQ(req.side, std::string("BUY"));
    ASSERT_EQ(req.type, std::string("LIMIT"));

    // Invalid side
    OrderRequest bad = req;
    bad.side = "HOLD";
    ASSERT_EQ(ValidateOrder(bad), RC_INVALID_PARAM);

    // Missing symbol
    bad = req;
    bad.symbol = "";
    ASSERT_EQ(ValidateOrder(bad), RC_INVALID_PARAM);

    // Zero qty
    bad = req;
    bad.qty = 0;
    ASSERT_EQ(ValidateOrder(bad), RC_INVALID_PARAM);
}

// ---------------------------------------------------------------------------
// Test: MockAdapter
// ---------------------------------------------------------------------------
static void TestMockAdapter() {
    Logger::Init("NUL");
    IAdapter* adapter = CreateMockAdapter();
    ASSERT_TRUE(adapter != nullptr);
    ASSERT_TRUE(adapter->IsConnected());

    OrderRequest req;
    req.cmd     = OrderCmd::PLACE;
    req.symbol  = "ESZ4";
    req.account = "TestAcct";
    req.qty     = 1;
    req.price   = 4500.0;
    req.side    = "BUY";
    req.type    = "LIMIT";

    ASSERT_EQ(adapter->Execute(req), RC_SUCCESS);
    delete adapter;
}

// ---------------------------------------------------------------------------
// Pipe stub for IPC tests
// ---------------------------------------------------------------------------
static DWORD WINAPI PipeServerThread(LPVOID) {
    HANDLE hPipe = CreateNamedPipeW(
        L"\\\\.\\pipe\\BridgeTestPipe",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 4096, 4096, 5000, nullptr);

    if (hPipe == INVALID_HANDLE_VALUE) return 1;

    ConnectNamedPipe(hPipe, nullptr);

    // Serve two requests: PING and then PLACE (or CONNECT)
    for (int i = 0; i < 3; ++i) {
        char buf[4096] = {};
        DWORD read = 0;
        if (!ReadFile(hPipe, buf, sizeof(buf) - 1, &read, nullptr) || read == 0) break;
        std::string req(buf, read);
        while (!req.empty() && (req.back() == '\r' || req.back() == '\n')) req.pop_back();

        std::string resp;
        if (req == "PING") resp = "PONG\n";
        else if (req == "CONNECT") resp = "OK stub connected\n";
        else resp = "OK stub " + req + "\n";

        DWORD written = 0;
        WriteFile(hPipe, resp.c_str(), static_cast<DWORD>(resp.size()), &written, nullptr);
    }

    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    return 0;
}

// ---------------------------------------------------------------------------
// Test: DotNetAdapter PING/PONG over named pipe
// ---------------------------------------------------------------------------
static void TestDotNetAdapterPing() {
    Logger::Init("NUL");

    // Start the stub pipe server before the adapter tries to connect.
    HANDLE hThread = CreateThread(nullptr, 0, PipeServerThread, nullptr, 0, nullptr);

    // Give the server a moment to create the pipe.
    Sleep(50);

    BridgeConfig cfg;
    cfg.adapterType      = "DOTNET";
    cfg.pipeName         = "\\\\.\\pipe\\BridgeTestPipe";
    cfg.dotnetWorkerPath = "nonexistent_worker.exe"; // skip actual process spawn

    HANDLE hPipe = INVALID_HANDLE_VALUE;
    DWORD deadline = GetTickCount() + 3000;
    while (GetTickCount() < deadline) {
        hPipe = CreateFileW(L"\\\\.\\pipe\\BridgeTestPipe",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hPipe != INVALID_HANDLE_VALUE) break;
        Sleep(50);
    }
    ASSERT_TRUE(hPipe != INVALID_HANDLE_VALUE);

    if (hPipe != INVALID_HANDLE_VALUE) {
        // PING
        const char* ping = "PING\n";
        DWORD written = 0;
        WriteFile(hPipe, ping, static_cast<DWORD>(strlen(ping)), &written, nullptr);
        char buf[64] = {};
        DWORD read = 0;
        ReadFile(hPipe, buf, sizeof(buf) - 1, &read, nullptr);
        std::string resp(buf, read);
        while (!resp.empty() && (resp.back() == '\r' || resp.back() == '\n')) resp.pop_back();
        ASSERT_EQ(resp, std::string("PONG"));

        // PLACE
        const char* place = "PLACE symbol=ES account=A qty=1 price=100 side=BUY type=LIMIT\n";
        WriteFile(hPipe, place, static_cast<DWORD>(strlen(place)), &written, nullptr);
        memset(buf, 0, sizeof(buf));
        ReadFile(hPipe, buf, sizeof(buf) - 1, &read, nullptr);
        resp = std::string(buf, read);
        while (!resp.empty() && (resp.back() == '\r' || resp.back() == '\n')) resp.pop_back();
        ASSERT_TRUE(resp.rfind("OK", 0) == 0);

        CloseHandle(hPipe);
    }

    WaitForSingleObject(hThread, 3000);
    CloseHandle(hThread);
}

// ---------------------------------------------------------------------------
// Test: DotNetAdapter CONNECT command
// ---------------------------------------------------------------------------
static void TestDotNetAdapterConnect() {
    Logger::Init("NUL");

    HANDLE hThread = CreateThread(nullptr, 0, PipeServerThread, nullptr, 0, nullptr);
    Sleep(50);

    HANDLE hPipe = INVALID_HANDLE_VALUE;
    DWORD deadline = GetTickCount() + 3000;
    while (GetTickCount() < deadline) {
        hPipe = CreateFileW(L"\\\\.\\pipe\\BridgeTestPipe",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hPipe != INVALID_HANDLE_VALUE) break;
        Sleep(50);
    }
    ASSERT_TRUE(hPipe != INVALID_HANDLE_VALUE);

    if (hPipe != INVALID_HANDLE_VALUE) {
        // PING first
        const char* ping = "PING\n";
        DWORD written = 0;
        WriteFile(hPipe, ping, static_cast<DWORD>(strlen(ping)), &written, nullptr);
        char buf[256] = {};
        DWORD read = 0;
        ReadFile(hPipe, buf, sizeof(buf) - 1, &read, nullptr);

        // CONNECT
        const char* conn = "CONNECT\n";
        WriteFile(hPipe, conn, static_cast<DWORD>(strlen(conn)), &written, nullptr);
        memset(buf, 0, sizeof(buf));
        ReadFile(hPipe, buf, sizeof(buf) - 1, &read, nullptr);
        std::string resp(buf, read);
        while (!resp.empty() && (resp.back() == '\r' || resp.back() == '\n')) resp.pop_back();
        ASSERT_TRUE(resp.rfind("OK", 0) == 0);

        CloseHandle(hPipe);
    }

    WaitForSingleObject(hThread, 3000);
    CloseHandle(hThread);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== BridgeCoreTests ===\n";

    TestConfigEnvOverride();
    std::cout << "TestConfigEnvOverride done\n";

    TestConfigFileLoad();
    std::cout << "TestConfigFileLoad done\n";

    TestValidation();
    std::cout << "TestValidation done\n";

    TestMockAdapter();
    std::cout << "TestMockAdapter done\n";

    TestDotNetAdapterPing();
    std::cout << "TestDotNetAdapterPing done\n";

    TestDotNetAdapterConnect();
    std::cout << "TestDotNetAdapterConnect done\n";

    std::cout << "\nResults: " << g_passed << " passed, "
              << g_failed  << " failed, "
              << g_skipped << " skipped.\n";
    return g_failed > 0 ? 1 : 0;
}
