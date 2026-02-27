#include <cstdio>
#include <windows.h>

// Load DLL dynamically for smoke testing
typedef int(__stdcall* FnPlaceOrderA)(
    const char*, const char*, const char*, const char*,
    int, const char*, double, double, const char*);

typedef int(__stdcall* FnPlaceOrderCmdA)(const char*);

int main() {
    printf("=== BridgeTestConsole Smoke Test ===\n");

    HMODULE hDll = LoadLibraryA("BridgeDLL.dll");
    if (!hDll) {
        // Try relative path for local runs
        hDll = LoadLibraryA("..\\..\\out\\Release\\BridgeDLL.dll");
    }
    if (!hDll) {
        printf("[SKIP] BridgeDLL.dll not found - smoke test skipped (DLL not in path).\n");
        return 0;
    }

    auto fnA = (FnPlaceOrderA)GetProcAddress(hDll, "PLACE_ORDER_A");
    auto fnCmdA = (FnPlaceOrderCmdA)GetProcAddress(hDll, "PLACE_ORDER_CMD_A");

    if (fnA) {
        int rc = fnA("PLACE", "ACC1", "ES", "BUY", 1, "MARKET", 0.0, 0.0, "DAY");
        printf("PLACE_ORDER_A -> %d (expected 0)\n", rc);
    } else {
        printf("[WARN] PLACE_ORDER_A not found in DLL\n");
    }

    if (fnCmdA) {
        int rc = fnCmdA("command=PLACE|account=ACC1|instrument=NQ|action=SELL|quantity=2|orderType=MARKET|limitPrice=0|stopPrice=0|timeInForce=DAY");
        printf("PLACE_ORDER_CMD_A (PLACE) -> %d (expected 0)\n", rc);

        rc = fnCmdA("command=CANCEL|account=ACC1|instrument=NQ|action=SELL|quantity=0|orderType=MARKET|limitPrice=0|stopPrice=0|timeInForce=DAY");
        printf("PLACE_ORDER_CMD_A (CANCEL) -> %d (expected 0)\n", rc);

        rc = fnCmdA("command=BADCMD");
        printf("PLACE_ORDER_CMD_A (BADCMD) -> %d (expected -1)\n", rc);
    } else {
        printf("[WARN] PLACE_ORDER_CMD_A not found in DLL\n");
    }

    FreeLibrary(hDll);
    printf("=== Smoke test complete ===\n");
    return 0;
}
