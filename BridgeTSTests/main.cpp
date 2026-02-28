// BridgeTSTests/main.cpp
// Console test runner that simulates TradeStation calling BridgeTS.dll.
// Loads the DLL dynamically, exercises all commands, and prints PASS/FAIL.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Return code constants (mirrors BridgeCore/include/Types.h)
// ---------------------------------------------------------------------------
static constexpr int RC_SUCCESS        =  0;
static constexpr int RC_INVALID_CMD    = -1;
static constexpr int RC_INVALID_PARAM  = -2;
static constexpr int RC_NOT_CONNECTED  = -3;
static constexpr int RC_INTERNAL_ERR   = -4;
static constexpr int RC_CONFIG_ERR     = -6;

// ---------------------------------------------------------------------------
// Function pointer type for PLACE_ORDER
// ---------------------------------------------------------------------------
using PLACE_ORDER_FN = int (__stdcall*)(
    const char*, const char*, const char*, const char*,
    int, const char*, double, double, const char*);

// ---------------------------------------------------------------------------
// Simple test counters & helpers
// ---------------------------------------------------------------------------
static int g_pass = 0;
static int g_fail = 0;

static void CheckEq(const char* label, int got, int expected)
{
    if (got == expected) {
        printf("[PASS] %s  (got %d)\n", label, got);
        ++g_pass;
    } else {
        printf("[FAIL] %s  (got %d, expected %d)\n", label, got, expected);
        ++g_fail;
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main()
{
    printf("=== BridgeTSTests ===\n\n");

    // ------------------------------------------------------------------
    // 1. Load BridgeTS.dll from the same directory as this executable.
    // ------------------------------------------------------------------
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    // Trim filename to get directory.
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    std::string dllPath = std::string(exePath) + "BridgeTS.dll";

    HMODULE hDll = LoadLibraryA(dllPath.c_str());
    if (!hDll) {
        printf("[FAIL] Could not load BridgeTS.dll from '%s' (error %lu)\n",
               dllPath.c_str(), GetLastError());
        return 1;
    }
    printf("Loaded: %s\n\n", dllPath.c_str());

    auto fnPlaceOrder = reinterpret_cast<PLACE_ORDER_FN>(
        GetProcAddress(hDll, "PLACE_ORDER"));
    if (!fnPlaceOrder) {
        printf("[FAIL] PLACE_ORDER export not found\n");
        FreeLibrary(hDll);
        return 1;
    }

    // ------------------------------------------------------------------
    // 2. Happy-path: PLACE MARKET BUY
    // ------------------------------------------------------------------
    printf("--- Happy-path orders ---\n");
    CheckEq("PLACE MARKET BUY",
        fnPlaceOrder("PLACE","ACC001","ESH26","BUY",1,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("PLACE MARKET SELL",
        fnPlaceOrder("PLACE","ACC001","ESH26","SELL",2,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("PLACE LIMIT BUY",
        fnPlaceOrder("PLACE","ACC001","ESH26","BUY",1,"LIMIT",4900.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("PLACE STOPLIMIT SELL GTC",
        fnPlaceOrder("PLACE","ACC001","ESH26","SELL",1,"STOPLIMIT",4800.0,4790.0,"GTC"),
        RC_SUCCESS);

    // ------------------------------------------------------------------
    // 3. All supported commands
    // ------------------------------------------------------------------
    printf("\n--- All 8 command strings ---\n");
    CheckEq("CANCEL",
        fnPlaceOrder("CANCEL","ACC001","ESH26","BUY",0,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("CANCELALLORDERS",
        fnPlaceOrder("CANCELALLORDERS","ACC001","ESH26","BUY",0,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("CHANGE",
        fnPlaceOrder("CHANGE","ACC001","ESH26","BUY",1,"LIMIT",4900.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("CLOSEPOSITION",
        fnPlaceOrder("CLOSEPOSITION","ACC001","ESH26","SELL",0,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("CLOSESTRATEGY",
        fnPlaceOrder("CLOSESTRATEGY","ACC001","ESH26","BUY",0,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("FLATTENEVERYTHING",
        fnPlaceOrder("FLATTENEVERYTHING","ACC001","ESH26","BUY",0,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    CheckEq("REVERSEPOSITION",
        fnPlaceOrder("REVERSEPOSITION","ACC001","ESH26","BUY",1,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    // Case-insensitive command
    CheckEq("place (lowercase)",
        fnPlaceOrder("place","ACC001","ESH26","BUY",1,"MARKET",0.0,0.0,"DAY"),
        RC_SUCCESS);

    // ------------------------------------------------------------------
    // 4. Invalid inputs
    // ------------------------------------------------------------------
    printf("\n--- Invalid / error inputs ---\n");

    // Null command
    CheckEq("null command -> RC_INVALID_PARAM",
        fnPlaceOrder(nullptr,"ACC001","ESH26","BUY",1,"MARKET",0.0,0.0,"DAY"),
        RC_INVALID_PARAM);

    // Null account
    CheckEq("null account -> RC_INVALID_PARAM",
        fnPlaceOrder("PLACE",nullptr,"ESH26","BUY",1,"MARKET",0.0,0.0,"DAY"),
        RC_INVALID_PARAM);

    // Null instrument
    CheckEq("null instrument -> RC_INVALID_PARAM",
        fnPlaceOrder("PLACE","ACC001",nullptr,"BUY",1,"MARKET",0.0,0.0,"DAY"),
        RC_INVALID_PARAM);

    // Unknown command
    CheckEq("BADCMD -> RC_INVALID_CMD",
        fnPlaceOrder("BADCMD","ACC001","ESH26","BUY",1,"MARKET",0.0,0.0,"DAY"),
        RC_INVALID_CMD);

    // Zero quantity for PLACE
    CheckEq("zero quantity for PLACE -> RC_INVALID_PARAM",
        fnPlaceOrder("PLACE","ACC001","ESH26","BUY",0,"MARKET",0.0,0.0,"DAY"),
        RC_INVALID_PARAM);

    // Unknown action
    CheckEq("BAD action -> RC_INVALID_PARAM",
        fnPlaceOrder("PLACE","ACC001","ESH26","HOLD",1,"MARKET",0.0,0.0,"DAY"),
        RC_INVALID_PARAM);

    // Unknown orderType
    CheckEq("BAD orderType -> RC_INVALID_PARAM",
        fnPlaceOrder("PLACE","ACC001","ESH26","BUY",1,"FUTURES",0.0,0.0,"DAY"),
        RC_INVALID_PARAM);

    // Unknown TIF
    CheckEq("BAD TIF -> RC_INVALID_PARAM",
        fnPlaceOrder("PLACE","ACC001","ESH26","BUY",1,"MARKET",0.0,0.0,"WEEK"),
        RC_INVALID_PARAM);

    // ------------------------------------------------------------------
    // 5. Clean up
    // ------------------------------------------------------------------
    FreeLibrary(hDll);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
