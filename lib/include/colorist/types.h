// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_TYPES_H
#define COLORIST_TYPES_H

// for size_t
#include <stddef.h>

// for uint*_t
#include <stdint.h>

// Gamma for sRGB (sRGB is not exactly 2.2, but is closest)
#define COLORIST_SRGB_GAMMA 2.2f

int clFileSize(const char * filename);

#define COLORIST_WARNING(MSG)                       \
    {                                               \
        clContextLogError(C, "WARNING: %s\n", MSG); \
    }

#ifdef COLORIST_DEBUG
#include <assert.h>
#define COLORIST_ASSERT assert
#define COLORIST_FAILURE(MSG)                       \
    {                                               \
        clContextLogError(C, "FAILURE: %s\n", MSG); \
        assert(0);                                  \
    }
#define COLORIST_FAILURE1(FMT, A)                      \
    {                                                  \
        clContextLogError(C, "FAILURE: " FMT "\n", A); \
        assert(0);                                     \
    }
#else
#define COLORIST_ASSERT(A)
#define COLORIST_FAILURE(MSG)
#define COLORIST_FAILURE1(FMT, A)
#endif

#define COLORIST_UNUSED(V) ((void)(V))

// Yes, clamp macros are nasty. Do not use them.
#define CL_CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

typedef int clBool;
#define clFalse 0
#define clTrue 1

typedef struct Timer
{
    double start;
} Timer;

void timerStart(Timer * timer);
double timerElapsedSeconds(Timer * timer);

uint16_t clHTONS(uint16_t s);
uint16_t clNTOHS(uint16_t s);
uint32_t clHTONL(uint32_t l);
uint32_t clNTOHL(uint32_t l);

#endif // ifndef COLORIST_TYPES_H
