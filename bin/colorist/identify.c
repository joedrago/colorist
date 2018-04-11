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
    Format format = args->format;
    if (format == FORMAT_AUTO)
        format = detectFormat(args->inputFilename);
    if (format == FORMAT_ERROR) {
        fprintf(stderr, "ERROR: Unknown output file format: %s\n", args->outputFilename);
        return 1;
    }

    printf("Colorist [identify]: %s\n", args->inputFilename);
    if (format == FORMAT_ICC) {
        clProfile * profile = clProfileRead(args->inputFilename);
        if (profile) {
            printf("Format: %s\n", formatToString(format));
            clProfileDebugDump(profile);
            clProfileDestroy(profile);
        }
    } else {
        clImage * image = readImage(args->inputFilename, &format);
        if (image) {
            printf("Format: %s\n", formatToString(format));
            clImageDebugDump(image, args->rect[0], args->rect[1], args->rect[2], args->rect[3]);
            clImageDestroy(image);
        }
    }
    return 0;
}
