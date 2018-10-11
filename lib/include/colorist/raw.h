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
    uint32_t size;
} clRaw;

typedef struct clStructArraySchema
{
    const char * format; // i8,i16,i32,u8,u16,u32,f32
    const char * name;
} clStructArraySchema;

struct clContext;

void clRawRealloc(struct clContext * C, clRaw * raw, uint32_t newSize);
void clRawFill(struct clContext * C, clRaw * raw, uint8_t fill);
void clRawClone(struct clContext * C, clRaw * dst, const clRaw * src);
clBool clRawDeflate(struct clContext * C, clRaw * dst, const clRaw * src);
char * clRawToBase64(struct clContext * C, clRaw * src);
void clRawSet(struct clContext * C, clRaw * raw, const uint8_t * data, uint32_t len);
void clRawFree(struct clContext * C, clRaw * raw);
clBool clRawReadFile(struct clContext * C, clRaw * raw, const char * filename);
clBool clRawWriteFile(struct clContext * C, clRaw * raw, const char * filename);
struct cJSON * clRawToStructArray(struct clContext * C, clRaw * raw, int width, int height, clStructArraySchema * schema, int schemaCount);

#endif
