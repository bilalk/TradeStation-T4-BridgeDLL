// TestFramework shared declarations (included by test files, not compiled separately)
#pragma once
#include <cstdio>

extern int g_pass;
extern int g_fail;

#define CHECK_EQ(a, b) do { \
    if ((a) == (b)) { \
        printf("[PASS] %s == %s\n", #a, #b); \
        ++g_pass; \
    } else { \
        printf("[FAIL] %s:%d  %s == %s  (got %d, expected %d)\n", \
               __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        ++g_fail; \
    } \
} while(0)

#define CHECK_STR_EQ(a, b) do { \
    if ((a) == (b)) { \
        printf("[PASS] %s == \"%s\"\n", #a, (b).c_str()); \
        ++g_pass; \
    } else { \
        printf("[FAIL] %s:%d  %s == \"%s\"  (got \"%s\")\n", \
               __FILE__, __LINE__, #a, (b).c_str(), (a).c_str()); \
        ++g_fail; \
    } \
} while(0)

#define CHECK_TRUE(x) do { \
    if ((x)) { \
        printf("[PASS] %s\n", #x); \
        ++g_pass; \
    } else { \
        printf("[FAIL] %s:%d  %s  (expected true)\n", __FILE__, __LINE__, #x); \
        ++g_fail; \
    } \
} while(0)

#define CHECK_FALSE(x) do { \
    if (!(x)) { \
        printf("[PASS] !(%s)\n", #x); \
        ++g_pass; \
    } else { \
        printf("[FAIL] %s:%d  !(%s)  (expected false)\n", __FILE__, __LINE__, #x); \
        ++g_fail; \
    } \
} while(0)
