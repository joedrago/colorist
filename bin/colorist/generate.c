// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include <string.h>

int actionGenerate(Args * args)
{
    Timer overall;
    clProfile * dstProfile = NULL;

    Format outputFileFormat = args->format;
    if (outputFileFormat == FORMAT_AUTO)
        outputFileFormat = detectFormat(args->outputFilename);
    if (outputFileFormat != FORMAT_ICC) {
        clLogError("generate can currently only make .icc files.");
        return 1;
    }

    clLog("action", 0, "Generating: %s", args->outputFilename);
    timerStart(&overall);

    if ((args->primaries[0] <= 0.0f) || (args->gamma <= 0.0f) || (args->luminance <= 0)) {
        clLogError("generate requires -p, -g, and -l.");
        return 1;
    }

    {
        clProfilePrimaries primaries;
        clProfileCurve curve;
        int luminance;
        char * description = NULL;

        primaries.red[0] = args->primaries[0];
        primaries.red[1] = args->primaries[1];
        primaries.green[0] = args->primaries[2];
        primaries.green[1] = args->primaries[3];
        primaries.blue[0] = args->primaries[4];
        primaries.blue[1] = args->primaries[5];
        primaries.white[0] = args->primaries[6];
        primaries.white[1] = args->primaries[7];
        curve.type = CL_PCT_GAMMA;
        curve.gamma = args->gamma;
        luminance = args->luminance;

        if (args->description) {
            description = strdup(args->description);
        } else {
            description = generateDescription(&primaries, &curve, luminance);
        }

        clLog("generate", 1, "Generating ICC profile: \"%s\"", description);
        dstProfile = clProfileCreate(&primaries, &curve, luminance, description);
        free(description);
        if (args->copyright) {
            clLog("generate", 1, "Setting copyright: \"%s\"", args->copyright);
            clProfileSetMLU(dstProfile, "cprt", "en", "US", args->copyright);
        }
    }

    // Dump out the profile to disk and bail out
    clLog("generate", 0, "Writing ICC: %s", args->outputFilename);
    clProfileDebugDump(dstProfile, 0);

    if (!clProfileWrite(dstProfile, args->outputFilename)) {
        return 1;
    }
    clProfileDestroy(dstProfile);

    clLog("action", 0, "\nGeneration complete (%g sec).", timerElapsedSeconds(&overall));
    return 0;
}

char * generateDescription(clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance)
{
    char * tmp = malloc(1024);
    sprintf(tmp, "Colorist P%g %gg %dnits", primaries->red[0], curve->gamma, maxLuminance);
    return tmp;
}
