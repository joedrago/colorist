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
    Format format;
    printf("Colorist [identify]: %s\n", args->inputFilename);
    image = readImage(args->inputFilename, &format);
    if (image != NULL) {
        printf("Format: %s\n", formatToString(format));
        clImageDebugDump(image, args->rect[0], args->rect[1], args->rect[2], args->rect[3]);
        clImageDestroy(image);
    }
    return 0;
}
