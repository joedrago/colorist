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

void clContextDefaultLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
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
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
}

void clContextDefaultLogError(clContext * C, const char * format, va_list args)
{
    fprintf(stderr, "** ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

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
