#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef BRIDGEDLL_EXPORTS
#define BRIDGE_API __declspec(dllexport)
#else
#define BRIDGE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Place order using ANSI symbol/account strings.
// Returns RC_SUCCESS (0) on success, non-zero on error.
BRIDGE_API int __stdcall PLACE_ORDER_A(
    const char* symbol,
    const char* account,
    double      qty,
    double      price,
    const char* side,
    const char* orderType);

// Place order using wide-character (Unicode) symbol/account strings.
BRIDGE_API int __stdcall PLACE_ORDER_W(
    const wchar_t* symbol,
    const wchar_t* account,
    double         qty,
    double         price,
    const wchar_t* side,
    const wchar_t* orderType);

// Place order from a single ANSI command string.
// Format: "SYMBOL ACCOUNT QTY PRICE SIDE TYPE"
// e.g.  "ESZ4 MyAcct 1 4500.0 BUY LIMIT"
BRIDGE_API int __stdcall PLACE_ORDER_CMD_A(const char* cmd);

// Place order from a single wide-char command string.
BRIDGE_API int __stdcall PLACE_ORDER_CMD_W(const wchar_t* cmd);

#ifdef __cplusplus
}
#endif
