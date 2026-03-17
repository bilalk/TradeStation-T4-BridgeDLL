#pragma once

// Export control is handled entirely by BridgeTS.def.
// Using __declspec(dllexport) on Win32 __stdcall bypasses the .DEF file
// and produces decorated names (_FUNCTIONNAME@N) that EasyLanguage cannot resolve.
#define BRIDGETS_API

extern "C" {

// ANSI entry point — primary interface for TradeStation EasyLanguage.
// Called via:  DefineDLLFunc: "BridgeTS.dll", INT, "PLACE_ORDER",
//              LPSTR, LPSTR, LPSTR, LPSTR, INT, LPSTR, DOUBLE, DOUBLE, LPSTR;
BRIDGETS_API int __stdcall PLACE_ORDER(
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
