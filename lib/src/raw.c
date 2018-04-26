// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/raw.h"

#include "colorist/context.h"

#include <string.h>

void clRawRealloc(struct clContext * C, clRaw * raw, uint32_t newSize)
{
    if (raw->size != newSize) {
        uint8_t * old = raw->ptr;
        uint32_t oldSize = raw->size;
        raw->ptr = clAllocate(newSize);
        raw->size = newSize;
        if (oldSize) {
            uint32_t bytesToCopy = (oldSize < raw->size) ? oldSize : raw->size;
            memcpy(raw->ptr, old, bytesToCopy);
            clFree(old);
        }
    }
}

void clRawFill(struct clContext * C, clRaw * raw, uint8_t fill)
{
    if (raw->ptr && raw->size) {
        memset(raw->ptr, raw->size, fill);
    }
}

void clRawClone(struct clContext * C, clRaw * dst, const clRaw * src)
{
    clRawSet(C, dst, dst->ptr, dst->size);
}

void clRawSet(struct clContext * C, clRaw * raw, const uint8_t * data, uint32_t len)
{
    if (len) {
        clRawRealloc(C, raw, len);
        memcpy(raw->ptr, data, len);
    } else {
        clRawFree(C, raw);
    }
}

void clRawFree(struct clContext * C, clRaw * raw)
{
    clFree(raw->ptr);
    raw->ptr = NULL;
    raw->size = 0;
}

#include <stdio.h>
#include <stdarg.h>

clBool clRawReadFile(struct clContext * C, clRaw * raw, const char * filename)
{
    int bytes;
    FILE * f;

    f = fopen(filename, "rb");
    if (!f) {
        clContextLogError(C, "Failed to open file for read: %s", filename);
        return clFalse;
    }
    fseek(f, 0, SEEK_END);
    bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    clRawRealloc(C, raw, bytes);
    if (fread(raw->ptr, raw->size, 1, f) != 1) {
        clContextLogError(C, "Failed to read file [%d bytes]: %s", (int)raw->size, filename);
        fclose(f);
        clRawFree(C, raw);
        return clFalse;
    }
    fclose(f);
    return clTrue;
}

int clFileSize(const char * filename)
{
    // TODO: reimplement as fstat()
    int bytes;

    FILE * f;
    f = fopen(filename, "rb");
    if (!f) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    fclose(f);
    return bytes;
}
