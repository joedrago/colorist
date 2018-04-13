// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/profile.h"

#include <string.h>

int clContextGenerate(clContext * C)
{
    Timer overall;
    clProfile * dstProfile = NULL;

    clFormat outputFileFormat = C->format;
    if (outputFileFormat == CL_FORMAT_AUTO)
        outputFileFormat = clFormatDetect(C, C->outputFilename);
    if (outputFileFormat != CL_FORMAT_ICC) {
        clContextLogError(C, "generate can currently only make .icc files.");
        return 1;
    }

    clContextLog(C, "action", 0, "Generating: %s", C->outputFilename);
    timerStart(&overall);

    if ((C->primaries[0] <= 0.0f) || (C->gamma <= 0.0f) || (C->luminance <= 0)) {
        clContextLogError(C, "generate requires -p, -g, and -l.");
        return 1;
    }

    {
        clProfilePrimaries primaries;
        clProfileCurve curve;
        int luminance;
        char * description = NULL;

        primaries.red[0] = C->primaries[0];
        primaries.red[1] = C->primaries[1];
        primaries.green[0] = C->primaries[2];
        primaries.green[1] = C->primaries[3];
        primaries.blue[0] = C->primaries[4];
        primaries.blue[1] = C->primaries[5];
        primaries.white[0] = C->primaries[6];
        primaries.white[1] = C->primaries[7];
        curve.type = CL_PCT_GAMMA;
        curve.gamma = C->gamma;
        luminance = C->luminance;

        if (C->description) {
            description = clContextStrdup(C, C->description);
        } else {
            description = clGenerateDescription(C, &primaries, &curve, luminance);
        }

        clContextLog(C, "generate", 1, "Generating ICC profile: \"%s\"", description);
        dstProfile = clProfileCreate(C, &primaries, &curve, luminance, description);
        clFree(description);
        if (C->copyright) {
            clContextLog(C, "generate", 1, "Setting copyright: \"%s\"", C->copyright);
            clProfileSetMLU(C, dstProfile, "cprt", "en", "US", C->copyright);
        }
    }

    // Dump out the profile to disk and bail out
    clContextLog(C, "generate", 0, "Writing ICC: %s", C->outputFilename);
    clProfileDebugDump(C, dstProfile, 0);

    if (!clProfileWrite(C, dstProfile, C->outputFilename)) {
        return 1;
    }
    clProfileDestroy(C, dstProfile);

    clContextLog(C, "action", 0, "\nGeneration complete (%g sec).", timerElapsedSeconds(&overall));
    return 0;
}
