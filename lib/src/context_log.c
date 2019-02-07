// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef COLORIST_EMSCRIPTEN

#include <emscripten.h>

void clContextDefaultLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    int needed = vsnprintf(NULL, 0, format, args);
    if (needed <= 0) {
        return;
    }

    char * buffer = clAllocate(needed + 1);
    vsnprintf(buffer, needed + 1, format, args);
    EM_ASM_({
        if (Module.coloristLog) {
            Module.coloristLog(UTF8ToString($0), $1, UTF8ToString($2));
        }
    }, section, indent, buffer);
#ifdef COLORIST_EMSCRIPTEN_ASYNC
    emscripten_sleep_with_yield(1);
#endif

    clFree(buffer);
}

void clContextDefaultLogError(clContext * C, const char * format, va_list args)
{
    int needed = vsnprintf(NULL, 0, format, args);
    if (needed <= 0) {
        return;
    }

    char * buffer = clAllocate(needed + 1);
    vsnprintf(buffer, needed + 1, format, args);
    EM_ASM_({
        if (Module.coloristError) {
            Module.coloristError(UTF8ToString($0));
        }
    }, buffer);
#ifdef COLORIST_EMSCRIPTEN_ASYNC
    emscripten_sleep_with_yield(1);
#endif

    clFree(buffer);
}

#else /* ifdef COLORIST_EMSCRIPTEN */

void clContextDefaultLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    COLORIST_UNUSED(C);

    if (section) {
        char spaces[10] = "         ";
        int spacesNeeded = 9 - (int)strlen(section);
        if (spacesNeeded < 0)
            spacesNeeded = 0;
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
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
}

void clContextDefaultLogError(clContext * C, const char * format, va_list args)
{
    COLORIST_UNUSED(C);

    fprintf(stderr, "** ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

#endif /* ifdef COLORIST_EMSCRIPTEN */

void clContextLog(clContext * C, const char * section, int indent, const char * format, ...)
{
    va_list args;
    va_start(args, format);
    C->system.log(C, section, indent, format, args);
    va_end(args);
}

void clContextLogError(clContext * C, const char * format, ...)
{
    va_list args;
    va_start(args, format);
    C->system.error(C, format, args);
    va_end(args);
}

void clContextLogWrite(clContext * C, const char * filename, const char * formatName, clWriteParams * writeParams)
{
    if (!formatName) {
        formatName = clFormatDetect(C, filename);
    }
    clFormat * format = clContextFindFormat(C, formatName);

    char yuvText[128];
    yuvText[0] = 0;
    if (format && format->usesYUVFormat) {
        sprintf(yuvText, " [YUV:%s]", clYUVFormatToString(C, writeParams->yuvFormat));
    }

    if (format && format->usesRate && format->usesQuality) {
        if ((writeParams->rate == 0) && (writeParams->quality == 100)) {
            clContextLog(C, "encode", 0, "Writing %s [Lossless]%s: %s", format->description, yuvText, filename);
        } else {
            clContextLog(C, "encode", 0, "Writing %s [%s:%d]%s: %s", format->description, (writeParams->rate) ? "R" : "Q", (writeParams->rate) ? writeParams->rate : writeParams->quality, yuvText, filename);
        }
    } else if (format && format->usesQuality) {
        if (writeParams->quality == 100) {
            clContextLog(C, "encode", 0, "Writing %s [Lossless]%s: %s", format->description, yuvText, filename);
        } else {
            clContextLog(C, "encode", 0, "Writing %s [Q:%d]%s: %s", format->description, writeParams->quality, yuvText, filename);
        }
    } else {
        clContextLog(C, "encode", 0, "Writing %s%s: %s", format->description, yuvText, filename);
    }
}
