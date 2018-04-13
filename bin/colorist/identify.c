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
        clLogError("Unknown file format: %s", args->inputFilename);
        return 1;
    }

    clLog("action", 0, "Identify: %s", args->inputFilename);
    if (format == FORMAT_ICC) {
        clProfile * profile = clProfileRead(args->inputFilename);
        if (profile) {
            clLog("identify", 1, "Format: %s", formatToString(format));
            clProfileDebugDump(profile, 1);
            clProfileDestroy(profile);
        }
    } else {
        clImage * image;
        clLog("decode", 0, "Reading: %s (%d bytes)", args->inputFilename, clFileSize(args->inputFilename));
        image = readImage(args->inputFilename, &format);
        if (image) {
            clLog("identify", 1, "Format: %s", formatToString(format));
            clImageDebugDump(image, args->rect[0], args->rect[1], args->rect[2], args->rect[3], 1);
            clImageDestroy(image);
        }
    }
    return 0;
}
