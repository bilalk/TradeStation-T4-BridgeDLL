#pragma once

// BridgeTS.h — TradeStation-facing DLL interface.
//
// NOTE: Do NOT use __declspec(dllexport) here.
// All exports are controlled exclusively by BridgeTS.def so that
// Win32 __stdcall name decoration (_PLACE_ORDER@44) is stripped.
// Adding dllexport overrides the .DEF file and re-introduces the
// mangled name, causing TradeStation EasyLanguage DefineDLLFunc to fail.

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