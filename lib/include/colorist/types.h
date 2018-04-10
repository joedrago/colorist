// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_TYPES_H
#define COLORIST_TYPES_H

// for calloc
#include <stdlib.h>

// for uint*_t
#include <stdint.h>

// for COLORIST_WARNING and COLORIST_FAILURE
#include <stdio.h>

#define COLORIST_WARNING(MSG) { fprintf(stderr, "WARNING: %s\n", MSG); }

#ifdef COLORIST_DEBUG
#include <assert.h>
#define COLORIST_ASSERT assert
#define COLORIST_FAILURE(MSG) { fprintf(stderr, "FAILURE: %s\n", MSG); assert(0); }
#define COLORIST_FAILURE1(FMT, A) { fprintf(stderr, "FAILURE: " FMT "\n", A); assert(0); }
#else
#define COLORIST_ASSERT(A)
#define COLORIST_FAILURE(MSG)
#define COLORIST_FAILURE1(FMT, A)
#endif

#define clAllocate(T) (T *)calloc(1, sizeof(T))

typedef struct clRaw
{
    uint8_t * ptr;
    uint32_t size;
} clRaw;

typedef int clBool;
#define clFalse 0
#define clTrue 1

void clRawRealloc(clRaw * raw, uint32_t newSize);
void clRawFill(clRaw * raw, uint8_t fill);
void clRawClone(clRaw * dst, const clRaw * src);
void clRawSet(clRaw * raw, const uint8_t * data, uint32_t len);
void clRawFree(clRaw * raw);

#endif // ifndef COLORIST_TYPES_H
