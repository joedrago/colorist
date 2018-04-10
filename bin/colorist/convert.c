// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include <string.h>

int actionConvert(Args * args)
{
    clImage * image = NULL;
    printf("Colorist [convert]: %s -> %s\n", args->inputFilename, args->outputFilename);
    image = readImage(args->inputFilename, NULL);
    if (image != NULL) {
        writeImage(image, args->outputFilename, FORMAT_AUTO);
        clImageDestroy(image);
    }
    return 0;
}
