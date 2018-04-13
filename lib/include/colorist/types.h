// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_TYPES_H
#define COLORIST_TYPES_H

// for uint*_t
#include <stdint.h>

// Output luminance colorist uses for basic profiles (sRGB, P3, etc)
#define COLORIST_DEFAULT_LUMINANCE 300

int clFileSize(const char * filename);

#define COLORIST_WARNING(MSG) { clContextLogError(C, "WARNING: %s\n", MSG); }

#ifdef COLORIST_DEBUG
#include <assert.h>
#define COLORIST_ASSERT assert
#define COLORIST_FAILURE(MSG) { clContextLogError(C, "FAILURE: %s\n", MSG); assert(0); }
#define COLORIST_FAILURE1(FMT, A) { clContextLogError(C, "FAILURE: " FMT "\n", A); assert(0); }
#else
#define COLORIST_ASSERT(A)
#define COLORIST_FAILURE(MSG)
#define COLORIST_FAILURE1(FMT, A)
#endif

// Yes, clamp macros are nasty. Do not use them.
#define CL_CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

typedef int clBool;
#define clFalse 0
#define clTrue 1

typedef struct clRaw
{
    uint8_t * ptr;
    uint32_t size;
} clRaw;

struct clContext;

void clRawRealloc(struct clContext * C, clRaw * raw, uint32_t newSize);
void clRawFill(struct clContext * C, clRaw * raw, uint8_t fill);
void clRawClone(struct clContext * C, clRaw * dst, const clRaw * src);
void clRawSet(struct clContext * C, clRaw * raw, const uint8_t * data, uint32_t len);
void clRawFree(struct clContext * C, clRaw * raw);

typedef struct Timer
{
    double start;
} Timer;

void timerStart(Timer * timer);
double timerElapsedSeconds(Timer * timer);

#endif // ifndef COLORIST_TYPES_H
