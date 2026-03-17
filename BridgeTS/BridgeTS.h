#pragma once

// BridgeTS.h — TradeStation-facing DLL interface.
//
// NOTE: Win32 builds do NOT define BRIDGETS_EXPORTS. BRIDGETS_API is empty,
// and all Win32 exports are controlled exclusively by BridgeTS.def so that
// the __stdcall decoration (_PLACE_ORDER@44) is stripped cleanly.
// x64 builds define BRIDGETS_EXPORTS (via vcxproj preprocessor defs), so
// BRIDGETS_API expands to __declspec(dllexport) on x64 (no @N issue there).

#ifdef BRIDGETS_EXPORTS
#  define BRIDGETS_API __declspec(dllexport)
#else
#  define BRIDGETS_API
#endif

extern "C" {

// ANSI entry point — primary interface for TradeStation 10 EasyLanguage.
// Called via:  DefineDLLFunc: "BridgeTS.dll", INT, "PLACE_ORDER",
//              LPSTR, LPSTR, LPSTR, LPSTR, INT, LPSTR, DOUBLE, DOUBLE, LPSTR;
int __stdcall PLACE_ORDER(
    const char* command,
    const char* account,
    const char* instrument,
    const char* action,
    int         quantity,
    const char* orderType,
    double      limitPrice,
    double      stopPrice,
    const char* timeInForce);

} // extern "C"