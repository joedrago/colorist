// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/types.h"

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

static double now();

void timerStart(Timer * timer)
{
    timer->start = now();
}

double timerElapsedSeconds(Timer * timer)
{
    return now() - timer->start;
}

#ifdef _WIN32
#include <windows.h>
static double now()
{
    return (double)GetTickCount() / 1000.0;
}
#else
#include <sys/time.h>
static double now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
}
#endif
