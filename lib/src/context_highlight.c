// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2020.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/image.h"
#include "colorist/profile.h"

#include "cJSON.h"

#include <string.h>

int clContextHighlight(clContext * C)
{
    COLORIST_UNUSED(C);

    const char * formatName = C->params.formatName;
    if (!formatName)
        formatName = clFormatDetect(C, C->inputFilename);
    if (!formatName) {
        clContextLogError(C, "Unknown file format: %s", C->inputFilename);
        return 1;
    }
    if (!strcmp(formatName, "icc")) {
        clContextLogError(C, "Highlights cannot output to ICC.");
        return 1;
    }

    clContextLog(C, "action", 0, "Highlight: %s", C->inputFilename);
    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    clImage * image = clContextRead(C, C->inputFilename, C->iccOverrideIn, &formatName);
    int ret = 1;
    if (image) {
        clImageDebugDump(C, image, 0, 0, 0, 0, 1);

        clImage * highlight = NULL;
        clImageHDRStats stats;
        clImageMeasureHDR(C, image, C->defaultLuminance, 0.0f, &highlight, &stats, NULL, NULL);
        if (highlight) {
            clContextLog(C, "highlight", 2, "Total Pixels          : %d", stats.pixelCount);
            clContextLog(C,
                         "highlight",
                         2,
                         "Overbright (%4d nits): %d (%2.2f%%)",
                         C->defaultLuminance,
                         stats.overbrightPixelCount,
                         100.0f * (float)stats.overbrightPixelCount / stats.pixelCount);
            clContextLog(C,
                         "highlight",
                         2,
                         "Out of Gamut (BT709)  : %d (%2.2f%%)",
                         stats.outOfGamutPixelCount,
                         100.0f * (float)stats.outOfGamutPixelCount / stats.pixelCount);
            clContextLog(C,
                         "highlight",
                         2,
                         "Both                  : %d (%2.2f%%)",
                         stats.hdrPixelCount,
                         100.0f * (float)stats.hdrPixelCount / stats.pixelCount);
            clContextLog(C,
                         "highlight",
                         2,
                         "Brightest Pixel       : %2.2f nits @ [%d, %d]",
                         stats.brightestPixelNits,
                         stats.brightestPixelX,
                         stats.brightestPixelY);

            clConversionParams params;
            memcpy(&params, &C->params, sizeof(params));
            params.writeParams.writeProfile = clFalse;

            clContextLogWrite(C, C->outputFilename, params.formatName, &params.writeParams);
            if (clContextWrite(C, highlight, C->outputFilename, params.formatName, &params.writeParams)) {
                ret = 0;
                clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
            }

            clImageDestroy(C, highlight);
        }
        clImageDestroy(C, image);
    }
    return ret;
}
