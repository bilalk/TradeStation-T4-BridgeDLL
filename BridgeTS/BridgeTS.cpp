// BridgeTS.cpp — TradeStation-facing shell for the T4 Bridge DLL.
// Receives calls from EasyLanguage, validates parameters, logs activity,
// and dispatches through BridgeCore (MOCK / DOTNET / FIX adapter).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "BridgeTS.h"

#include "BridgeEngine.h"
#include "Parser.h"
#include "Logger.h"
#include "Types.h"

#include <string>
#include <atomic>

// ---------------------------------------------------------------------------
// Module-level request counter (used to tag log lines with [REQ-NNNN])
// ---------------------------------------------------------------------------
namespace {

static std::atomic<unsigned int> g_reqCounter{ 0 }; 

// Format a request tag such as "[REQ-0001]"
static std::string ReqTag(unsigned int id)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "[REQ-%04u]", id);
    return buf;
}

// SEH-guarded engine execute — no C++ objects in this function.
static int SEH_Execute(Bridge::BridgeEngine& engine, const Bridge::OrderRequest& req)
{
    __try {
        return engine.Execute(req);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return Bridge::RC_INTERNAL_ERR;
    }
}

// Core dispatch — all public entry points converge here after building req.
static int DispatchRequest(const Bridge::OrderRequest& req, const std::string& tag)
{
    int rc = SEH_Execute(Bridge::GetEngine(), req);
    if (rc == Bridge::RC_INTERNAL_ERR) {
        Bridge::LogError(tag + " SEH exception in DispatchRequest");
    } else {
        Bridge::LogDebug(tag + " Execute returned rc=" + std::to_string(rc));
    }
    return rc;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason, LPVOID /*lpReserved*/)
{
    switch (ul_reason) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Exported functions
// ---------------------------------------------------------------------------
extern "C" {

// Primary ANSI entry point — this is the function Fred wires up in EasyLanguage.
BRIDGETS_API int __stdcall PLACE_ORDER(
    const char* command,
    const char* account,
    const char* instrument,
    const char* action,
    int         quantity,
    const char* orderType,
    double      limitPrice,
    double      stopPrice,
    const char* timeInForce)
{
    unsigned int id = ++g_reqCounter;
    std::string tag = ReqTag(id);

    // Guard against null command — return RC_INVALID_PARAM per spec.
    if (!command) {
        Bridge::LogWarning(tag + " PLACE_ORDER called with null command");
        return Bridge::RC_INVALID_PARAM;
    }

    Bridge::LogInfo(tag + " PLACE_ORDER called"
        " command=" + std::string(command ? command : "<null>") +
        " account=" + std::string(account ? account : "<null>") +
        " instrument=" + std::string(instrument ? instrument : "<null>") +
        " action=" + std::string(action ? action : "<null>") +
        " quantity=" + std::to_string(quantity) +
        " orderType=" + std::string(orderType ? orderType : "<null>") +
        " limitPrice=" + std::to_string(limitPrice) +
        " stopPrice=" + std::to_string(stopPrice) +
        " tif=" + std::string(timeInForce ? timeInForce : "<null>"));

    Bridge::OrderRequest req;
    int rc = Bridge::BuildRequest(command, account, instrument, action,
                                  quantity, orderType, limitPrice, stopPrice,
                                  timeInForce, req);
    if (rc != Bridge::RC_SUCCESS) {
        Bridge::LogWarning(tag + " Validation failed rc=" + std::to_string(rc));
        return rc;
    }
    Bridge::LogInfo(tag + " Validation: OK");
    return DispatchRequest(req, tag);
}

} // extern "C"