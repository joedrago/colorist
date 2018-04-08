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
