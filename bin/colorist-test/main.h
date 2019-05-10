// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

// Helpers
void setFloat4(float c[4], float v0, float v1, float v2, float v3);
void setFloat3(float c[3], float v0, float v1, float v2);
void setRGBA8_4(uint8_t c[4], uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3);
void setRGBA8_3(uint8_t c[3], uint8_t v0, uint8_t v1, uint8_t v2);
void setRGBA16_4(uint16_t c[4], uint16_t v0, uint16_t v1, uint16_t v2, uint16_t v3);
void setRGBA16_3(uint16_t c[3], uint16_t v0, uint16_t v1, uint16_t v2);

// For silencing test output
extern clContextSystem silentSystem;

// Test suites, named after their associated .c file
int test_coverage(void);
int test_io(void);
int test_strings(void);
