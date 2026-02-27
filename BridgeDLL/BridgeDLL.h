#pragma once

#ifdef BRIDGEDLL_EXPORTS
#  define BRIDGE_API __declspec(dllexport)
#else
#  define BRIDGE_API __declspec(dllimport)
#endif

extern "C" {

// Multi-argument Unicode variant
BRIDGE_API int __stdcall PLACE_ORDER_W(
    const wchar_t* command,
    const wchar_t* account,
    const wchar_t* instrument,
    const wchar_t* action,
    int            quantity,
    const wchar_t* orderType,
    double         limitPrice,
    double         stopPrice,
    const wchar_t* timeInForce);

// Multi-argument ANSI variant
BRIDGE_API int __stdcall PLACE_ORDER_A(
    const char* command,
    const char* account,
    const char* instrument,
    const char* action,
    int         quantity,
    const char* orderType,
    double      limitPrice,
    double      stopPrice,
    const char* timeInForce);

// Single pipe-delimited Unicode payload
BRIDGE_API int __stdcall PLACE_ORDER_CMD_W(const wchar_t* payload);

// Single pipe-delimited ANSI payload
BRIDGE_API int __stdcall PLACE_ORDER_CMD_A(const char* payload);

} // extern "C"
