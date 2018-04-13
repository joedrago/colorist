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

int clContextIdentify(clContext * C)
{
    clFormat format = C->format;
    if (format == CL_FORMAT_AUTO)
        format = clFormatDetect(C, C->inputFilename);
    if (format == CL_FORMAT_ERROR) {
        clContextLogError(C, "Unknown file format: %s", C->inputFilename);
        return 1;
    }

    clContextLog(C, "action", 0, "Identify: %s", C->inputFilename);
    if (format == CL_FORMAT_ICC) {
        clProfile * profile = clProfileRead(C, C->inputFilename);
        if (profile) {
            clContextLog(C, "identify", 1, "Format: %s", clFormatToString(C, format));
            clProfileDebugDump(C, profile, 1);
            clProfileDestroy(C, profile);
        }
    } else {
        clImage * image;
        clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
        image = clContextRead(C, C->inputFilename, &format);
        if (image) {
            clContextLog(C, "identify", 1, "Format: %s", clFormatToString(C, format));
            clImageDebugDump(C, image, C->rect[0], C->rect[1], C->rect[2], C->rect[3], 1);
            clImageDestroy(C, image);
        }
    }
    return 0;
}
