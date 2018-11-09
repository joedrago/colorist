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
    clFormat * format;
    const char * formatName = clFormatDetect(C, filename);
    if (outFormatName)
        *outFormatName = formatName;
    if (!formatName) {
        return NULL;
    }

    clRaw input = CL_RAW_EMPTY;
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

    if (formatName == NULL) {
        formatName = clFormatDetect(C, filename);
        if (formatName == NULL) {
            clContextLogError(C, "Unknown output file format '%s', please specify with -f", filename);
            return clFalse;
        }
    }

    clFormat * format = clContextFindFormat(C, formatName);
    COLORIST_ASSERT(format);

    clWriteParams writeParams;
    writeParams.quality = quality;
    writeParams.rate = rate;

    if (format->writeFunc) {
        clRaw output = CL_RAW_EMPTY;
        if (format->writeFunc(C, image, formatName, &output, &writeParams)) {
            if (clRawWriteFile(C, &output, filename)) {
                result = clTrue;
            }
        }
        clRawFree(C, &output);
    } else {
        clContextLogError(C, "Unimplemented file writer '%s'", formatName);
    }
    return result;
}

char * clContextWriteURI(struct clContext * C, clImage * image, const char * formatName, int quality, int rate)
{
    char * output = NULL;

    clFormat * format = clContextFindFormat(C, formatName);
    if (!format) {
        clContextLogError(C, "Unknown format: %s", formatName);
        return NULL;
    }

    clWriteParams writeParams;
    writeParams.quality = quality;
    writeParams.rate = rate;

    if (format->writeFunc) {
        clRaw dst = CL_RAW_EMPTY;
        if (format->writeFunc(C, image, formatName, &dst, &writeParams)) {
            char prefix[512];
            size_t prefixLen = sprintf(prefix, "data:%s;base64,", format->mimeType);

            char * b64 = clRawToBase64(C, &dst);
            if (!b64) {
                clRawFree(C, &dst);
                return NULL;
            }
            size_t b64Len = strlen(b64);

            output = clAllocate(prefixLen + b64Len + 1);
            memcpy(output, prefix, prefixLen);
            memcpy(output + prefixLen, b64, b64Len);
            output[prefixLen + b64Len] = 0;

            clFree(b64);
        }
        clRawFree(C, &dst);
    } else {
        clContextLogError(C, "Unimplemented file writer '%s'", formatName);
    }

    return output;
}
