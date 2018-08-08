// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef COLORIST_EMSCRIPTEN

#include <emscripten.h>

void clContextDefaultLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    int needed;
    char * buffer;
    needed = vsnprintf(NULL, 0, format, args);
    if (needed <= 0) {
        return;
    }

    buffer = clAllocate(needed + 1);
    vsnprintf(buffer, needed + 1, format, args);
    EM_ASM_({
        if (Module.coloristLog) {
            Module.coloristLog(UTF8ToString($0), $1, UTF8ToString($2));
        }
    }, section, indent, buffer);
    emscripten_sleep_with_yield(1);

    clFree(buffer);
}

void clContextDefaultLogError(clContext * C, const char * format, va_list args)
{
    int needed;
    char * buffer;
    needed = vsnprintf(NULL, 0, format, args);
    if (needed <= 0) {
        return;
    }

    buffer = clAllocate(needed + 1);
    vsnprintf(buffer, needed + 1, format, args);
    EM_ASM_({
        if (Module.coloristError) {
            Module.coloristError(UTF8ToString($0));
        }
    }, buffer);
    emscripten_sleep_with_yield(1);

    clFree(buffer);
}

#else /* ifdef COLORIST_EMSCRIPTEN */

void clContextDefaultLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    if (section) {
        char spaces[10] = "         ";
        int spacesNeeded = 9 - strlen(section);
        spacesNeeded = CL_CLAMP(spacesNeeded, 0, 9);
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
