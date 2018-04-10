// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include <string.h>

int actionIdentify(Args * args)
{
    clImage * image = NULL;
    printf("Colorist [identify]: %s\n", args->inputFilename);
    Format format = detectFormat(args->inputFilename);
    if (format == FORMAT_ERROR) {
        return 1;
    }
    printf("Format: %s\n", formatToString(format));
    switch (format) {
        case FORMAT_PNG:
            image = clImageLoadPNG(args->inputFilename);
            break;
        default:
            fprintf(stderr, "ERROR: Unimplemented file loader '%s'\n", formatToString(format));
            break;
    }
    if (image != NULL) {
        clImageDebugDump(image);
    }
    return 0;
}
