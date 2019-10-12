// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_RAW_H
#define COLORIST_RAW_H

#include "colorist/types.h"

typedef struct clRaw
{
    uint8_t * ptr;
    size_t size;
} clRaw;

#define CL_RAW_EMPTY \
    {                \
        NULL, 0      \
    }

struct clContext;

void clRawRealloc(struct clContext * C, clRaw * raw, size_t newSize);
void clRawClone(struct clContext * C, clRaw * dst, const clRaw * src);
clBool clRawDeflate(struct clContext * C, clRaw * dst, const clRaw * src);
char * clRawToBase64(struct clContext * C, clRaw * src);
void clRawSet(struct clContext * C, clRaw * raw, const uint8_t * data, size_t len);
void clRawFree(struct clContext * C, clRaw * raw);
clBool clRawReadFile(struct clContext * C, clRaw * raw, const char * filename);
clBool clRawReadFileHeader(struct clContext * C, clRaw * raw, const char * filename, size_t bytes);
clBool clRawWriteFile(struct clContext * C, clRaw * raw, const char * filename);

#endif
