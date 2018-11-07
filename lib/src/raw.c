// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/raw.h"

#include "colorist/context.h"

#include "cJSON.h"
#include "zlib.h"
#include <string.h>

void clRawRealloc(struct clContext * C, clRaw * raw, size_t newSize)
{
    if (raw->size != newSize) {
        uint8_t * old = raw->ptr;
        size_t oldSize = raw->size;
        raw->ptr = clAllocate(newSize);
        raw->size = newSize;
        if (oldSize) {
            size_t bytesToCopy = (oldSize < raw->size) ? oldSize : raw->size;
            memcpy(raw->ptr, old, bytesToCopy);
            clFree(old);
        }
    }
}

void clRawClone(struct clContext * C, clRaw * dst, const clRaw * src)
{
    clRawSet(C, dst, src->ptr, src->size);
}

clBool clRawDeflate(struct clContext * C, clRaw * dst, const clRaw * src)
{
    clBool ret = clTrue;
    z_stream z;
    int err;
    size_t maxDeflatedSize = src->size + 6 + (((src->size + 16383) / 16384) * 5);
    clRawRealloc(C, dst, maxDeflatedSize);

    memset(&z, 0, sizeof(z));
    z.total_in = (uLong)src->size;
    z.avail_in = (uInt)src->size;
    z.total_out = (uLong)dst->size;
    z.avail_out = (uInt)dst->size;
    z.next_in = src->ptr;
    z.next_out = dst->ptr;

    err = deflateInit(&z, Z_DEFAULT_COMPRESSION);
    if (err == Z_OK) {
        err = deflate(&z, Z_FINISH);
        if (err == Z_STREAM_END) {
            dst->size = z.total_out;
        } else {
            clContextLogError(C, "failed to decompress %d bytes!", src->size);
            ret = clFalse;
        }
    }
    deflateEnd(&z);

    if (!ret) {
        clRawFree(C, dst);
    }
    return ret;
}

// Original implementation copyright:

/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

// Main changes to the original are to add clAllocate and remove line feeds.

static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char * clRawToBase64(struct clContext * C, clRaw * src)
{
    unsigned char * out, * pos;
    const unsigned char * end, * in;
    size_t olen;
    // int line_len;

    olen = src->size * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
    // olen += olen / 72;            /* line feeds */
    olen++; /* nul termination */
    if (olen < src->size)
        return NULL; /* integer overflow */
    out = clAllocate(olen);
    if (out == NULL)
        return NULL;

    end = src->ptr + src->size;
    in = src->ptr;
    pos = out;
    // line_len = 0;
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
        // line_len += 4;
        // if (line_len >= 72) {
        //     *pos++ = '\n';
        //     line_len = 0;
        // }
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                                  (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
        // line_len += 4;
    }

// if (line_len)
//     *pos++ = '\n';

    *pos = '\0';
    return (char *)out;
}

void clRawSet(struct clContext * C, clRaw * raw, const uint8_t * data, size_t len)
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

struct cJSON * clRawToStructArray(struct clContext * C, clRaw * raw, int width, int height, clStructArraySchema * schema, int schemaCount)
{
    cJSON * json = cJSON_CreateObject();
    cJSON * jsonSchema = cJSON_CreateArray();
    clRaw compressed;
    char * b64;
    int i;

    cJSON_AddItemToObject(json, "width", cJSON_CreateNumber(width));
    cJSON_AddItemToObject(json, "height", cJSON_CreateNumber(height));

    memset(&compressed, 0, sizeof(compressed));
    clRawDeflate(C, &compressed, raw);
    b64 = clRawToBase64(C, &compressed);
    cJSON_AddItemToObject(json, "data", cJSON_CreateString(b64));
    clFree(b64);
    clRawFree(C, &compressed);

    cJSON_AddItemToObject(json, "schema", jsonSchema);
    for (i = 0; i < schemaCount; ++i) {
        cJSON * jsonEntry = cJSON_CreateObject();
        cJSON_AddItemToObject(jsonEntry, "format", cJSON_CreateString(schema[i].format));
        cJSON_AddItemToObject(jsonEntry, "name", cJSON_CreateString(schema[i].name));
        cJSON_AddItemToArray(jsonSchema, jsonEntry);
    }
    return json;
}

#include <stdio.h>
#include <stdarg.h>

clBool clRawReadFile(struct clContext * C, clRaw * raw, const char * filename)
{
    long bytes;
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

clBool clRawWriteFile(struct clContext * C, clRaw * raw, const char * filename)
{
    FILE * f;

    f = fopen(filename, "wb");
    if (!f) {
        clContextLogError(C, "Failed to open file for write: %s", filename);
        return clFalse;
    }
    if (raw->size > 0) {
        if (fwrite(raw->ptr, raw->size, 1, f) != 1) {
            fclose(f);
            clContextLogError(C, "Failed to write %d bytes to: %s", raw->size, filename);
            return clFalse;
        }
    }
    fclose(f);
    return clTrue;
}

int clFileSize(const char * filename)
{
    // TODO: reimplement as fstat()
    long bytes;

    FILE * f;
    f = fopen(filename, "rb");
    if (!f) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    fclose(f);
    return (int)bytes;
}
