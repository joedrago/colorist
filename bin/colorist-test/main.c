// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

// #define WIN32_MEMORY_LEAK_DETECTION
#ifdef WIN32_MEMORY_LEAK_DETECTION
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "main.h"

// --------------------------------------------------------------------------------------
// Helpers

void setFloat4(float c[4], float v0, float v1, float v2, float v3)
{
    c[0] = v0;
    c[1] = v1;
    c[2] = v2;
    c[3] = v3;
}
void setFloat3(float c[3], float v0, float v1, float v2)
{
    c[0] = v0;
    c[1] = v1;
    c[2] = v2;
}
void setRGBA8_4(uint8_t c[4], uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3)
{
    c[0] = v0;
    c[1] = v1;
    c[2] = v2;
    c[3] = v3;
}
void setRGBA8_3(uint8_t c[3], uint8_t v0, uint8_t v1, uint8_t v2)
{
    c[0] = v0;
    c[1] = v1;
    c[2] = v2;
}
void setRGBA16_4(uint16_t c[4], uint16_t v0, uint16_t v1, uint16_t v2, uint16_t v3)
{
    c[0] = v0;
    c[1] = v1;
    c[2] = v2;
    c[3] = v3;
}
void setRGBA16_3(uint16_t c[3], uint16_t v0, uint16_t v1, uint16_t v2)
{
    c[0] = v0;
    c[1] = v1;
    c[2] = v2;
}

clContextSystem silentSystem;
static void clContextSilentLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(section);
    COLORIST_UNUSED(indent);
    COLORIST_UNUSED(format);
    COLORIST_UNUSED(args);
}
static void clContextSilentLogError(clContext * C, const char * format, va_list args)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(format);
    COLORIST_UNUSED(args);
}

// --------------------------------------------------------------------------------------
// Unity stubs

void setUp(void)
{
}
void tearDown(void)
{
}
void suiteSetUp(void)
{
}
int suiteTearDown(int num_failures)
{
    return num_failures;
}

// --------------------------------------------------------------------------------------
// Main / List of active tests

static clBool shouldRun(const char * cmdlineName, int argc, char * argv[])
{
    if (argc < 2) {
        // run everything
        return clTrue;
    }

    // Only run names on the cmdline
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(cmdlineName, argv[i])) {
            return clTrue;
        }
    }
    return clFalse;
}

#define RUN_TESTS(TESTS, CMDLINENAME, TITLE)      \
    do {                                          \
        if (shouldRun(CMDLINENAME, argc, argv)) { \
            printf("_______________________\n");  \
            printf("%s\n", TITLE);                \
            printf("-----------------------\n");  \
            int ret = TESTS();                    \
            if (ret != 0)                         \
                return ret;                       \
        }                                         \
    } while (0)

int main(int argc, char * argv[])
{
#ifdef WIN32_MEMORY_LEAK_DETECTION
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(260);
#endif

    // Lots of these tests will be purposefully spewing errors or requesting conversions,
    // there's no need to muddy stdout with colorist yapping away on most of them.
    silentSystem.alloc = clContextDefaultAlloc;
    silentSystem.free = clContextDefaultFree;
    silentSystem.log = clContextSilentLog;
    silentSystem.error = clContextSilentLogError;

    RUN_TESTS(test_coverage, "coverage", "Coverage");
    RUN_TESTS(test_io, "io", "I/O");
    RUN_TESTS(test_strings, "strings", "Image Strings");

    return 0;
}
