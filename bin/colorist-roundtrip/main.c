// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include "colorist/transform.h"

#include <stdlib.h>
#include <string.h>

static int countCodePointDiffs(uint16_t c1[3], uint16_t c2[3])
{
    int diffs = 0;
    int i;
    for (i = 0; i < 3; ++i) {
        if (c1[i] > c2[i]) {
            diffs += c1[i] - c2[i];
        } else {
            diffs += c2[i] - c1[i];
        }
    }
    return diffs;
}

static void roundtrip(clContext * C, int depth, clProfile * profile, clProfile * intermediateProfile, clBool whiteOnly)
{
    typedef float Pattern[3];

    clTransform * srcToInt;
    clTransform * intToDst;
    uint16_t src16[3];
    uint16_t dst16[3];
    float intermediate[3];
    int maxChannel = (1 << depth) - 1;
    int channelIndex, patternIndex;

    int totalMismatches = 0;
    int totalDiffs = 0;
    int totalAttempts = 0;
    int highestDiff = 0;

    // Attempting to hit all 12bit color patterns is 2^36 possible transformations. Ain't nobody got time for that.
    // These are some interesting color patterns that are easy to run through various combinations of.
    Pattern whitePatterns[] = { { 1, 1, 1 } };
    Pattern colorPatterns[] = {
        { 1, 0, 0 },   // Red
        { 1, 0.5, 0 }, // Orange
        { 1, 1, 0 },   // Yellow
        { 0, 1, 0 },   // Green
        { 0, 0, 1 },   // Blue
        { 1, 0, 1 },   // Magenta
        { 0, 1, 1 },   // Cyan

        { 1, 1, 1 }
    };
    const int colorPatternsCount = sizeof(colorPatterns) / sizeof(colorPatterns[0]);

    srcToInt = clTransformCreate(C, profile, CL_XF_RGB, depth, intermediateProfile, CL_XF_RGB, 32, CL_TONEMAP_OFF);
    intToDst = clTransformCreate(C, intermediateProfile, CL_XF_RGB, 32, profile, CL_XF_RGB, depth, CL_TONEMAP_OFF);
    for (channelIndex = 0; channelIndex <= maxChannel; ++channelIndex) {
        Pattern * patterns;
        int patternsCount;
        if (whiteOnly) {
            patterns = whitePatterns;
            patternsCount = 1;
        } else {
            patterns = colorPatterns;
            patternsCount = colorPatternsCount;
        }

        for (patternIndex = 0; patternIndex < patternsCount; ++patternIndex, ++totalAttempts) {
            int diffs;
            float * pattern = patterns[patternIndex];

            src16[0] = (uint16_t)((float)channelIndex * pattern[0]);
            src16[1] = (uint16_t)((float)channelIndex * pattern[1]);
            src16[2] = (uint16_t)((float)channelIndex * pattern[2]);

            clTransformRun(C, srcToInt, 1, src16, intermediate, 1);
            clTransformRun(C, intToDst, 1, intermediate, dst16, 1);
            diffs = countCodePointDiffs(src16, dst16);
            if (diffs > 0) {
                ++totalMismatches;
                totalDiffs += diffs;
                if (highestDiff < diffs) {
                    highestDiff = diffs;
                }
#if 0
                printf("(%u,%u,%u) -> (%g,%g,%g) -> (%u,%u,%u)\n",
                    src16[0], src16[1], src16[2],
                    intermediate[0], intermediate[1], intermediate[2],
                    dst16[0], dst16[1], dst16[2]);
#endif
            }
        }
    }
    clTransformDestroy(C, srcToInt);
    clTransformDestroy(C, intToDst);

    {
        float avgDiff = (totalMismatches > 0) ? ((float)totalDiffs / (float)totalMismatches) : 0;
        printf("[%s -> %s -> %s] (%s): %d/%d changed, highestDiff: %d avgDiff: %g\n",
            profile->description, intermediateProfile->description, profile->description,
            whiteOnly ? "whites" : "colors",
            totalMismatches, totalAttempts, highestDiff, avgDiff);
    }
}

int main(int argc, char * argv[])
{
    clContext * C = clContextCreate(NULL);
    struct clProfile * BT2020_G1;
    struct clProfile * BT2020_PQ;
    struct clProfile * BT709_100;
    struct clProfile * BT709_300;
    clProfilePrimaries primaries;
    clProfileCurve curve;

    // Create BT2020 profiles
    clContextGetStockPrimaries(C, "bt2020", &primaries);
    curve.type = CL_PCT_GAMMA;
    curve.gamma = 1.0f;
    BT2020_G1 = clProfileCreate(C, &primaries, &curve, 10000, "BT2020 10k G1");
    BT2020_PQ = clProfileRead(C, "../docs/profiles/HDR_UHD_ST2084.icc");
    if (!BT2020_PQ)
        return 0;

    // Create BT.709 profiles
    clContextGetStockPrimaries(C, "bt709", &primaries);
    curve.type = CL_PCT_GAMMA;
    curve.gamma = 2.2f;
    BT709_100 = clProfileCreate(C, &primaries, &curve, 100, "BT709 100 G22");
    BT709_300 = clProfileCreate(C, &primaries, &curve, 300, "BT709 300 G22");

    // Do some roundtrips
    roundtrip(C, 12, BT2020_PQ, BT2020_G1, clTrue);
    roundtrip(C, 12, BT2020_PQ, BT2020_G1, clFalse);
    roundtrip(C, 12, BT709_100, BT2020_PQ, clTrue);
    roundtrip(C, 12, BT709_100, BT2020_PQ, clFalse);
    roundtrip(C, 12, BT709_300, BT2020_PQ, clTrue);
    roundtrip(C, 12, BT709_300, BT2020_PQ, clFalse);

    // Cleanup
    clProfileDestroy(C, BT2020_G1);
    clProfileDestroy(C, BT2020_PQ);
    clProfileDestroy(C, BT709_100);
    clProfileDestroy(C, BT709_300);
    clContextDestroy(C);

    printf("colorist-roundtrip Complete.\n");
    return 0;
}
