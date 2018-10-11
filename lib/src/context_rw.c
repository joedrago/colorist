// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/image.h"
#include "colorist/profile.h"

#include <string.h>

struct clImage * clContextRead(clContext * C, const char * filename, const char * iccOverride, const char ** outFormatName)
{
    clImage * image = NULL;
    clRaw input;
    clFormat * format;
    const char * formatName = clFormatDetect(C, filename);
    if (outFormatName)
        *outFormatName = formatName;
    if (!formatName) {
        return NULL;
    }

    memset(&input, 0, sizeof(input));
    if (!clRawReadFile(C, &input, filename)) {
        return clFalse;
    }

    format = clContextFindFormat(C, formatName);
    COLORIST_ASSERT(format);
    if (format->readFunc) {
        image = format->readFunc(C, formatName, &input);
    } else {
        clContextLogError(C, "Unimplemented file reader '%s'", formatName);
    }

    if (image && iccOverride) {
        clProfile * overrideProfile = clProfileRead(C, iccOverride);
        if (overrideProfile) {
            clContextLog(C, "profile", 1, "Overriding src profile with file: %s", iccOverride);
            clProfileDestroy(C, image->profile);
            image->profile = overrideProfile; // take ownership
        } else {
            clContextLogError(C, "Bad ICC override file [-i]: %s", iccOverride);
            clImageDestroy(C, image);
            image = NULL;
        }
    }

    clRawFree(C, &input);
    return image;
}

clBool clContextWrite(clContext * C, struct clImage * image, const char * filename, const char * formatName, int quality, int rate)
{
    clBool result = clFalse;
    clFormat * format;
    clWriteParams writeParams;
    clRaw output;

    memset(&writeParams, 0, sizeof(writeParams));
    memset(&output, 0, sizeof(output));

    if (formatName == NULL) {
        formatName = clFormatDetect(C, filename);
        if (formatName == NULL) {
            clContextLogError(C, "Unknown output file format '%s', please specify with -f", filename);
            return clFalse;
        }
    }

    format = clContextFindFormat(C, formatName);
    COLORIST_ASSERT(format);

    writeParams.quality = quality;
    writeParams.rate = rate;
    if (format->writeFunc) {
        if (format->writeFunc(C, image, formatName, &output, &writeParams)) {
            if (clRawWriteFile(C, &output, filename)) {
                result = clTrue;
            }
        }
    } else {
        clContextLogError(C, "Unimplemented file writer '%s'", formatName);
    }
    clRawFree(C, &output);
    return result;
}

char * clContextWriteURI(struct clContext * C, clImage * image, const char * formatName, int quality, int rate)
{
    clRaw dst;
    char * b64;
    int b64Len;
    char * output;
    clWriteParams writeParams;

    clFormat * format = clContextFindFormat(C, formatName);
    if (!format) {
        clContextLogError(C, "Unknown format: %s", formatName);
        return NULL;
    }

    memset(&dst, 0, sizeof(dst));

    writeParams.quality = quality;
    writeParams.rate = rate;
    if (format->writeFunc) {
        if (format->writeFunc(C, image, formatName, &dst, &writeParams)) {
            char prefix[512];
            int prefixLen = sprintf(prefix, "data:%s;base64,", format->mimeType);

            b64 = clRawToBase64(C, &dst);
            if (!b64) {
                clRawFree(C, &dst);
                return NULL;
            }
            b64Len = strlen(b64);

            output = clAllocate(prefixLen + b64Len + 1);
            memcpy(output, prefix, prefixLen);
            memcpy(output + prefixLen, b64, b64Len);
            output[prefixLen + b64Len] = 0;

            clFree(b64);
        }
    } else {
        clContextLogError(C, "Unimplemented file writer '%s'", formatName);
    }

    clRawFree(C, &dst);
    return output;
}
