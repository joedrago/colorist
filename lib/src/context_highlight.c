// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"
#include "colorist/image.h"
#include "colorist/types.h"

#include <string.h>

#define FAIL()              \
    {                       \
        returnCode = 1;     \
        goto reportCleanup; \
    }

int clContextHighlight(clContext * C)
{
    Timer overall, t;
    int returnCode = 0;

    clContextLog(C, "action", 0, "Highlight: %s -> %s", C->inputFilename, C->outputFilename);
    timerStart(&overall);

    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    timerStart(&t);
    clImage * image = clContextRead(C, C->inputFilename, C->iccOverrideIn, NULL);
    if (image == NULL) {
        return 1;
    }
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    timerStart(&t);
    {
        clImage * highlight = NULL;
        clImageHDRStats highlightStats;
        clImageMeasureHDR(C, image, C->defaultLuminance, 0.0f, &highlight, &highlightStats, NULL, NULL);
        if (!highlight) {
            FAIL();
        }
        clWriteParams writeParams;
        clWriteParamsSetDefaults(C, &writeParams);
        if (!clContextWrite(C, highlight, C->outputFilename, "png", &writeParams)) {
            FAIL();
        }
    }

    clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

reportCleanup:
    if (image)
        clImageDestroy(C, image);

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "Highlight complete.");
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
}
