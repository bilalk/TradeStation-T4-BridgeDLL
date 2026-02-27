#include "TestFramework.h"

// Definitions of shared test counters
int g_pass = 0;
int g_fail = 0;

// Forward declarations for test functions
void TestValidation();
void TestParser();
void TestMockAdapter();

int main() {
    printf("=== BridgeCoreTests ===\n\n");

    TestValidation();
    TestParser();
    TestMockAdapter();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
