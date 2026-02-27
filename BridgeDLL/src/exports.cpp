#include "exports.h"
#include "../../BridgeCore/include/Config.h"
#include "../../BridgeCore/include/IAdapter.h"
#include "../../BridgeCore/include/Logger.h"
#include "../../BridgeCore/include/OrderRequest.h"
#include "../../BridgeCore/include/ResultCodes.h"

#include <string>
#include <memory>
#include <mutex>
#include <sstream>

// Forward declarations of adapter factories in BridgeCore.
IAdapter* CreateMockAdapter();
IAdapter* CreateDotNetAdapter(const BridgeConfig& cfg);
int       ValidateOrder(OrderRequest& req);

// ---------------------------------------------------------------------------
// Singleton state
// ---------------------------------------------------------------------------
static std::once_flag  g_initFlag;
static BridgeConfig    g_config;
static IAdapter*       g_adapter = nullptr;

static void EnsureInitialised() {
    std::call_once(g_initFlag, []() {
        // Try config relative to the DLL, then CWD.
        bool loaded = g_config.LoadFromFile("config\\bridge.json");
        if (!loaded) loaded = g_config.LoadFromFile("bridge.json");
        g_config.ApplyEnvOverrides();

        Logger::Init(g_config.logPath);
        Logger::Log("BridgeDLL initialised. adapterType=" + g_config.adapterType);

        if (g_config.adapterType == "DOTNET") {
            g_adapter = CreateDotNetAdapter(g_config);
        } else {
            g_adapter = CreateMockAdapter();
        }
    });
}

// ---------------------------------------------------------------------------
// Internal execute helper
// ---------------------------------------------------------------------------
static int ExecuteOrder(OrderRequest& req) {
    EnsureInitialised();
    int rc = ValidateOrder(req);
    if (rc != RC_SUCCESS) {
        Logger::Log("ValidateOrder failed rc=" + std::to_string(rc));
        return rc;
    }
    return g_adapter->Execute(req);
}

// ---------------------------------------------------------------------------
// ANSI / wide conversion helpers
// ---------------------------------------------------------------------------
static std::string W2A(const wchar_t* w) {
    if (!w) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
    return s;
}

// ---------------------------------------------------------------------------
// Export implementations
// ---------------------------------------------------------------------------
extern "C" {

BRIDGE_API int __stdcall PLACE_ORDER_A(
    const char* symbol, const char* account,
    double qty, double price,
    const char* side, const char* orderType)
{
    if (!symbol || !account || !side || !orderType) return RC_INVALID_PARAM;
    OrderRequest req;
    req.cmd     = OrderCmd::PLACE;
    req.symbol  = symbol;
    req.account = account;
    req.qty     = qty;
    req.price   = price;
    req.side    = side;
    req.type    = orderType;
    return ExecuteOrder(req);
}

BRIDGE_API int __stdcall PLACE_ORDER_W(
    const wchar_t* symbol, const wchar_t* account,
    double qty, double price,
    const wchar_t* side, const wchar_t* orderType)
{
    if (!symbol || !account || !side || !orderType) return RC_INVALID_PARAM;
    return PLACE_ORDER_A(
        W2A(symbol).c_str(), W2A(account).c_str(),
        qty, price,
        W2A(side).c_str(), W2A(orderType).c_str());
}

BRIDGE_API int __stdcall PLACE_ORDER_CMD_A(const char* cmd) {
    if (!cmd) return RC_INVALID_PARAM;
    // Parse: "SYMBOL ACCOUNT QTY PRICE SIDE TYPE"
    std::istringstream ss(cmd);
    OrderRequest req;
    req.cmd = OrderCmd::PLACE;
    std::string qty, price;
    if (!(ss >> req.symbol >> req.account >> qty >> price >> req.side >> req.type))
        return RC_INVALID_PARAM;
    try {
        req.qty   = std::stod(qty);
        req.price = std::stod(price);
    } catch (...) { return RC_INVALID_PARAM; }
    return ExecuteOrder(req);
}

BRIDGE_API int __stdcall PLACE_ORDER_CMD_W(const wchar_t* cmd) {
    if (!cmd) return RC_INVALID_PARAM;
    return PLACE_ORDER_CMD_A(W2A(cmd).c_str());
}

} // extern "C"
