// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/image.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/task.h"

#include <string.h>

#include "reportTemplateBinary.h"

#define FAIL() { returnCode = 1; goto reportCleanup; }

int clContextReport(clContext * C)
{
    Timer overall, t;
    int returnCode = 0;

    clImage * srcImage = NULL;

    FILE * outf = NULL;

    clContextLog(C, "action", 0, "Report: %s -> %s", C->inputFilename, C->outputFilename);
    timerStart(&overall);

    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    timerStart(&t);
    srcImage = clContextRead(C, C->inputFilename, NULL);
    if (srcImage == NULL) {
        return 1;
    }
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    timerStart(&t);
    {
        outf = fopen(C->outputFilename, "wb");
        if (!outf) {
            clContextLogError(C, "Cant open report file for write: %s", C->outputFilename);
            FAIL();
        }
        fwrite(reportTemplateBinaryData, reportTemplateBinarySize, 1, outf);
        fclose(outf);
        outf = NULL;
    }

    clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

reportCleanup:
    if (srcImage)
        clImageDestroy(C, srcImage);
    if (outf)
        fclose(outf);

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "Conversion complete.");
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
}
