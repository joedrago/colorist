// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/types.h"

#include <string.h>

void clRawRealloc(clRaw * raw, uint32_t newSize)
{
    raw->ptr = realloc(raw->ptr, newSize);
    raw->size = newSize;
}

void clRawFill(clRaw * raw, uint8_t fill)
{
    if (raw->ptr && raw->size) {
        memset(raw->ptr, raw->size, fill);
    }
}

void clRawClone(clRaw * dst, const clRaw * src)
{
    clRawSet(dst, dst->ptr, dst->size);
}

void clRawSet(clRaw * raw, const uint8_t * data, uint32_t len)
{
    if (len) {
        clRawRealloc(raw, len);
        memcpy(raw->ptr, data, len);
    } else {
        clRawFree(raw);
    }
}

void clRawFree(clRaw * raw)
{
    free(raw->ptr);
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

void clLog(const char * section, int indent, const char * format, ...)
{
    va_list args;

    if (section) {
        char spaces[9] = "        ";
        int spacesNeeded = 8 - strlen(section);
        spacesNeeded = CL_CLAMP(spacesNeeded, 0, 8);
        spaces[spacesNeeded] = 0;
        fprintf(stdout, "[%s%s] ", spaces, section);
    }
    if (indent < 0)
        indent = 17 + indent;
    if (indent > 0) {
        int i;
        for (i = 0; i < indent; ++i) {
            fprintf(stdout, "    ");
        }
    }
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fprintf(stdout, "\n");
}

void clLogError(const char * format, ...)
{
    va_list args;
    fprintf(stderr, "** ERROR: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
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
