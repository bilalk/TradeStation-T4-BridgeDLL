#define BRIDGEDLL_EXPORTS
#include "../BridgeDLL.h"
#include "../../BridgeCore/include/BridgeEngine.h"
#include "../../BridgeCore/include/Parser.h"
#include "../../BridgeCore/include/Logger.h"
#include "../../BridgeCore/include/Types.h"
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

static std::string WideToUtf8(const wchar_t* w) {
    if (!w) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
    return s;
}

} // anonymous namespace

extern "C" {

BRIDGE_API int __stdcall PLACE_ORDER_W(
    const wchar_t* command,
    const wchar_t* account,
    const wchar_t* instrument,
    const wchar_t* action,
    int            quantity,
    const wchar_t* orderType,
    double         limitPrice,
    double         stopPrice,
    const wchar_t* timeInForce)
{
    try {
        Bridge::OrderRequest req;
        int rc = Bridge::BuildRequest(command, account, instrument, action,
                                      quantity, orderType, limitPrice, stopPrice,
                                      timeInForce, req);
        if (rc != Bridge::RC_SUCCESS) return rc;
        return Bridge::GetEngine().Execute(req);
    }
    catch (...) {
        Bridge::LogError("Unhandled exception in PLACE_ORDER_W");
        return Bridge::RC_INTERNAL_ERR;
    }
}

BRIDGE_API int __stdcall PLACE_ORDER_A(
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
    try {
        Bridge::OrderRequest req;
        int rc = Bridge::BuildRequest(command, account, instrument, action,
                                      quantity, orderType, limitPrice, stopPrice,
                                      timeInForce, req);
        if (rc != Bridge::RC_SUCCESS) return rc;
        return Bridge::GetEngine().Execute(req);
    }
    catch (...) {
        Bridge::LogError("Unhandled exception in PLACE_ORDER_A");
        return Bridge::RC_INTERNAL_ERR;
    }
}

BRIDGE_API int __stdcall PLACE_ORDER_CMD_W(const wchar_t* payload)
{
    try {
        std::string narrow = WideToUtf8(payload);
        Bridge::OrderRequest req;
        int rc = Bridge::ParsePayload(narrow, req);
        if (rc != Bridge::RC_SUCCESS) return rc;
        return Bridge::GetEngine().Execute(req);
    }
    catch (...) {
        Bridge::LogError("Unhandled exception in PLACE_ORDER_CMD_W");
        return Bridge::RC_INTERNAL_ERR;
    }
}

BRIDGE_API int __stdcall PLACE_ORDER_CMD_A(const char* payload)
{
    try {
        Bridge::OrderRequest req;
        int rc = Bridge::ParsePayload(payload ? payload : "", req);
        if (rc != Bridge::RC_SUCCESS) return rc;
        return Bridge::GetEngine().Execute(req);
    }
    catch (...) {
        Bridge::LogError("Unhandled exception in PLACE_ORDER_CMD_A");
        return Bridge::RC_INTERNAL_ERR;
    }
}

} // extern "C"
