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

static void setFloat4(float c[4], float v0, float v1, float v2, float v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
static void setFloat3(float c[3], float v0, float v1, float v2) { c[0] = v0; c[1] = v1; c[2] = v2; }
static void setRGBA8_4(uint8_t c[4], uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
static void setRGBA8_3(uint8_t c[3], uint8_t v0, uint8_t v1, uint8_t v2) { c[0] = v0; c[1] = v1; c[2] = v2; }
static void setRGBA16_4(uint16_t c[4], uint16_t v0, uint16_t v1, uint16_t v2, uint16_t v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
static void setRGBA16_3(uint16_t c[3], uint16_t v0, uint16_t v1, uint16_t v2) { c[0] = v0; c[1] = v1; c[2] = v2; }

// #define DEBUG_SINGLE_DIFF

static int diffTransform(clContext * C, int steps, clProfile * srcProfile, clTransformFormat srcFormat, int srcDepth, clProfile * dstProfile, clTransformFormat dstFormat, int dstDepth, clTonemap tonemap, float thresholdF)
{
#if defined(DEBUG_SINGLE_DIFF)
    clBool printProgress = clTrue;
    clBool printMatches = clFalse;
#else
    clBool printProgress = clFalse;
    clBool printMatches = clFalse;
#endif
    clBool printMismatches = clFalse;

    int diffCount = 0;

    float srcFloat[4];
    float dstFloatCCMM[4];
    float dstFloatLCMS[4];

    uint8_t src8[4];
    uint8_t dst8CCMM[4];
    uint8_t dst8LCMS[4];

    uint16_t src16[4];
    uint16_t dst16CCMM[4];
    uint16_t dst16LCMS[4];

    void * srcRaw;
    void * dstRawCCMM;
    void * dstRawLCMS;

    float channelDiv = (float)(steps - 1);
    int srcMaxChannel = 0;
    float srcScale = 1.0f;
    int r, g, b;

    int thresholdI = (int)thresholdF;

    char title[512];
    char errorPrefix[512];

    clTransform * transform = clTransformCreate(C, srcProfile, srcFormat, srcDepth, dstProfile, dstFormat, dstDepth, tonemap);

    if (srcDepth <= 16) {
        srcMaxChannel = (1 << srcDepth) - 1;
        srcScale = (float)srcMaxChannel;
    }

    sprintf(title, "'%s' (%d bit, %s) --%s--> '%s' (%d bit, %s)",
        srcProfile ? srcProfile->description : "XYZ", srcDepth, (srcFormat == CL_XF_RGBA) ? "alpha" : "noalpha",
        (tonemap == CL_TONEMAP_AUTO) ? "auto" : (tonemap == CL_TONEMAP_ON) ? "tonemap" : "clip",
        dstProfile ? dstProfile->description : "XYZ", dstDepth, (dstFormat == CL_XF_RGBA) ? "alpha" : "noalpha");
    if (printProgress) {
        strcpy(errorPrefix, " * ");
        printf("%s - ->\n", title);
    } else {
        strcpy(errorPrefix, title);
        strcat(errorPrefix, " - ");
    }

    for (r = 0; r < steps; ++r) {
        if (printProgress) {
            printf(" * %s - %d/%d\n", title, r + 1, steps);
        }
        for (g = 0; g < steps; ++g) {
            for (b = 0; b < steps; ++b) {
                clBool differs = clFalse;

                setFloat4(srcFloat, r / channelDiv, g / channelDiv, b / channelDiv, 1.0f);

                if (srcDepth == 32) {
                    srcRaw = srcFloat;
                } else if (srcDepth > 8) {
                    setRGBA16_4(src16,
                        (uint16_t)clPixelMathRoundf(srcFloat[0] * srcScale),
                        (uint16_t)clPixelMathRoundf(srcFloat[1] * srcScale),
                        (uint16_t)clPixelMathRoundf(srcFloat[2] * srcScale),
                        srcMaxChannel
                        );
                    srcRaw = src16;
                } else {
                    setRGBA8_4(src8,
                        (uint8_t)clPixelMathRoundf(srcFloat[0] * srcScale),
                        (uint8_t)clPixelMathRoundf(srcFloat[1] * srcScale),
                        (uint8_t)clPixelMathRoundf(srcFloat[2] * srcScale),
                        255
                        );
                    srcRaw = src8;
                }

                if (dstDepth == 32) {
                    dstRawCCMM = dstFloatCCMM;
                    dstRawLCMS = dstFloatLCMS;
                } else if (dstDepth > 8) {
                    dstRawCCMM = dst16CCMM;
                    dstRawLCMS = dst16LCMS;
                } else {
                    dstRawCCMM = dst8CCMM;
                    dstRawLCMS = dst8LCMS;
                }

                C->ccmmAllowed = clTrue;
                clTransformRun(C, transform, 1, srcRaw, dstRawCCMM, 1);
                C->ccmmAllowed = clFalse;
                clTransformRun(C, transform, 1, srcRaw, dstRawLCMS, 1);

                if (dstDepth == 32) {
                    if ((fabsf(dstFloatCCMM[0] - dstFloatLCMS[0]) > thresholdF)
                        || (fabsf(dstFloatCCMM[1] - dstFloatLCMS[1]) > thresholdF)
                        || (fabsf(dstFloatCCMM[2] - dstFloatLCMS[2]) > thresholdF))
                    {
                        differs = clTrue;
                    }
                } else if (dstDepth > 8) {
                    if ((abs((int)dst16CCMM[0] - (int)dst16LCMS[0]) > thresholdI)
                        || (abs((int)dst16CCMM[1] - (int)dst16LCMS[1]) > thresholdI)
                        || (abs((int)dst16CCMM[2] - (int)dst16LCMS[2]) > thresholdI))
                    {
                        differs = clTrue;
                    }
                } else {
                    if ((abs((int)dst8CCMM[0] - (int)dst8LCMS[0]) > thresholdI)
                        || (abs((int)dst8CCMM[1] - (int)dst8LCMS[1]) > thresholdI)
                        || (abs((int)dst8CCMM[2] - (int)dst8LCMS[2]) > thresholdI))
                    {
                        differs = clTrue;
                    }
                }

                if (differs) {
                    ++diffCount;
                }

                if ((printMatches && !differs) || (printMismatches && differs)) {
                    const char * prefix = "Match";
                    if (differs) {
                        prefix = "Mismatch";
                    }
                    if (srcDepth == 32) {
                        printf("%s%s: SRC(%g,%g,%g)", errorPrefix, prefix, srcFloat[0], srcFloat[1], srcFloat[2]);
                    } else if (srcDepth > 8) {
                        printf("%s%s: SRC(%u,%u,%u)", errorPrefix, prefix, src16[0], src16[1], src16[2]);
                    } else {
                        printf("%s%s: SRC(%u,%u,%u)", errorPrefix, prefix, src8[0], src8[1], src8[2]);
                    }

                    if (dstDepth == 32) {
                        printf(" CCMM(%g,%g,%g) LCMS(%g,%g,%g)\n",
                            dstFloatCCMM[0], dstFloatCCMM[1], dstFloatCCMM[2],
                            dstFloatLCMS[0], dstFloatLCMS[1], dstFloatLCMS[2]);
                    } else if (dstDepth > 8) {
                        printf(" CCMM(%u,%u,%u) LCMS(%u,%u,%u)\n",
                            dst16CCMM[0], dst16CCMM[1], dst16CCMM[2],
                            dst16LCMS[0], dst16LCMS[1], dst16LCMS[2]);
                    } else {
                        printf(" CCMM(%u,%u,%u) LCMS(%u,%u,%u)\n",
                            dst8CCMM[0], dst8CCMM[1], dst8CCMM[2],
                            dst8LCMS[0], dst8LCMS[1], dst8LCMS[2]);
                    }

                    // if (differs) {
                    // goto bailOut;
                    // }
                }
            }
        }
    }

// bailOut:

    if (diffCount > 0) {
        printf("%s - %d differences\n", title, diffCount);
    }

    clTransformDestroy(C, transform);
    return diffCount;
}

#define FAIL() printf("ERROR: found mismatches, bailing out\n"); goto foundMismatch;

int main(int argc, char * argv[])
{
    clContext * C = clContextCreate(NULL);
    clProfilePrimaries primaries;
    clProfileCurve curve;

#if defined(DEBUG_MATRIX_MATH)
    {
        clProfile * bt709;
        clProfile * bt2020;
        clTransform * transform;
        C = clContextCreate(NULL);

        clContextGetStockPrimaries(C, "bt709", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 1.0f;
        bt709 = clProfileCreate(C, &primaries, &curve, 0, NULL);

        clContextGetStockPrimaries(C, "bt2020", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 1.0f;
        bt2020 = clProfileCreate(C, &primaries, &curve, 0, NULL);

        transform = clTransformCreate(C, bt709, CL_XF_RGBA, 32, bt2020, CL_XF_RGBA, 32, CL_TONEMAP_OFF);
        clTransformPrepare(C, transform);
        clTransformDestroy(C, transform);

        clContextDestroy(C);
        return 0;
    }
#endif

    static const int steps = 16;

    struct clProfile * BT709;
    struct clProfile * BT2020;
    struct clProfile * P3PQ;
    struct clProfile * profiles[4];
    int profilesCount = 0; // set later

    // const int depths[]     = { 8, 9, 10, 11, 12, 13, 14, 15, 16, 32 };
    const int depths[]     = { 8, 16, 32 };
    const int depthsCount = sizeof(depths) / sizeof(depths[0]);
    int srcProfileIndex, srcDepthIndex, dstProfileIndex, dstDepthIndex, tonemapIndex;

    C = clContextCreate(NULL);

    curve.type = CL_PCT_GAMMA;
    curve.gamma = 2.2f;

    clContextGetStockPrimaries(C, "bt709", &primaries);
    BT709 = clProfileCreate(C, &primaries, &curve, 300, "BT709 300 G22");
    clContextGetStockPrimaries(C, "bt2020", &primaries);
    BT2020 = clProfileCreate(C, &primaries, &curve, 10000, "BT2020 10k G22");
    P3PQ = clProfileRead(C, "../docs/profiles/HDR_P3_D65_ST2084.icc");
    if (!P3PQ) {
        return -1;
    }
    profiles[0] = BT2020;
    profiles[1] = BT709;
    profiles[2] = P3PQ;
    profiles[3] = NULL; // XYZ
    profilesCount = 4;

#if defined(DEBUG_SINGLE_DIFF)
    diffTransform(C, steps, P3PQ, CL_XF_RGB, 16, BT2020, CL_XF_RGB, 16, CL_TONEMAP_OFF, 1);
#else
    for (srcProfileIndex = 0; srcProfileIndex < profilesCount; ++srcProfileIndex) {
        for (srcDepthIndex = 0; srcDepthIndex < depthsCount; ++srcDepthIndex) {
            for (dstProfileIndex = 0; dstProfileIndex < profilesCount; ++dstProfileIndex) {
                for (dstDepthIndex = 0; dstDepthIndex < depthsCount; ++dstDepthIndex) {
                    for (tonemapIndex = 0; tonemapIndex < 3; ++tonemapIndex) {
                        clProfile * srcProfile = profiles[srcProfileIndex];
                        int srcDepth = depths[srcDepthIndex];
                        clTransformFormat srcFormat = CL_XF_RGB;

                        clProfile * dstProfile = profiles[dstProfileIndex];
                        int dstDepth = depths[dstDepthIndex];
                        clTransformFormat dstFormat = CL_XF_RGB;

                        clTonemap tonemap = (clTonemap)tonemapIndex; // Naughty!
                        float threshold = 0.00001f;
                        if (dstDepth < 32) {
                            threshold = 1.0f;
                        }

                        if (srcProfile == NULL) {
                            if (srcDepth == 32) {
                                srcFormat = CL_XF_XYZ;
                            } else {
                                // Only do 32bit XYZ
                                continue;
                            }
                        }

                        if (dstProfile == NULL) {
                            if (dstDepth == 32) {
                                dstFormat = CL_XF_XYZ;
                            } else {
                                // Only do 32bit XYZ
                                continue;
                            }
                        }

                        if (diffTransform(C, steps, srcProfile, srcFormat, srcDepth, dstProfile, dstFormat, dstDepth, tonemap, threshold) > 0) {
                            // FAIL();
                        }
                        if (srcFormat == CL_XF_RGB) {
                            if (diffTransform(C, steps, srcProfile, CL_XF_RGBA, srcDepth, dstProfile, dstFormat, dstDepth, tonemap, threshold) > 0) {
                                // FAIL();
                            }
                        }
                        if (dstFormat == CL_XF_RGB) {
                            if (diffTransform(C, steps, srcProfile, srcFormat, srcDepth, dstProfile, CL_XF_RGBA, dstDepth, tonemap, threshold) > 0) {
                                // FAIL();
                            }
                        }
                        if ((srcFormat == CL_XF_RGB) && (dstFormat == CL_XF_RGB)) {
                            if (diffTransform(C, steps, srcProfile, CL_XF_RGBA, srcDepth, dstProfile, CL_XF_RGBA, dstDepth, tonemap, threshold) > 0) {
                                // FAIL();
                            }
                        }
                    }
                }
            }
        }
    }

// foundMismatch:
#endif /* if 0 */

    clProfileDestroy(C, BT709);
    clProfileDestroy(C, P3PQ);
    clProfileDestroy(C, BT2020);
    clContextDestroy(C);

    printf("colorist-test Complete.\n");
    return 0;
}
