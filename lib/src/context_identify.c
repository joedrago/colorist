// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/image.h"
#include "colorist/profile.h"

#include "cJSON.h"

#include <string.h>

int clContextIdentify(clContext * C, struct cJSON * output)
{
    const char * formatName = C->params.formatName;
    if (!formatName)
        formatName = clFormatDetect(C, C->inputFilename);
    if (!formatName) {
        clContextLogError(C, "Unknown file format: %s", C->inputFilename);
        return 1;
    }

    clContextLog(C, "action", 0, "Identify: %s", C->inputFilename);
    if (!strcmp(formatName, "icc")) {
        clProfile * profile = clProfileRead(C, C->inputFilename);
        if (profile) {
            clContextLog(C, "identify", 1, "Format: %s", formatName);
            if (output) {
                clProfileDebugDumpJSON(C, output, profile, C->verbose);
            } else {
                clProfileDebugDump(C, profile, C->verbose, 1);
            }
            clProfileDestroy(C, profile);
        }
    } else {
        clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
        clImage * image = clContextRead(C, C->inputFilename, C->iccOverrideIn, &formatName);
        if (image) {
            int rect[4];
            memcpy(rect, C->params.rect, sizeof(rect));
            // if ((rect[2] < 0) && (rect[3] < 0)) {
            //     // Defaults for identify
            //     rect[2] = 3;
            //     rect[3] = 3;
            // }
            clContextLog(C, "identify", 1, "Format: %s", formatName);
            if (output) {
                clImageDebugDumpJSON(C, output, image, rect[0], rect[1], rect[2], rect[3]);
            } else {
                clImageDebugDump(C, image, rect[0], rect[1], rect[2], rect[3], 1);
            }
            clImageDestroy(C, image);
        }
    }
    return 0;
}
