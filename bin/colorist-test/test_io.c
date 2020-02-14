// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2019.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

// #define DEBUG_TEST_IMAGES 1

#define TEST_IMAGE_STRING \
    "256x256,#ff0000,#00ff00,#0000ff,#ffff00,#ff00ff,#00ffff,#000000,#ffffff,#990000,#009900,#000099,#999900,#990099,#009999,#000000,#999999"

struct clExtInfo
{
    char * ext;
    int threshold;
    char * bpc[8];
    char * quality[8];
    char * primaries[8];
    char * curve[8];
    char * luminance[8];
    char * yuv[8];
};

static void test_images(const char * ext, int threshold, int additionalArgc, char ** additionalArgv)
{
    int argc;
    char * argv[32];
    char tmpFilename[128];

    argc = 0;
    argv[argc++] = "colorist";
    argv[argc++] = "generate";
    argv[argc++] = TEST_IMAGE_STRING;

    for (int i = 0; i < additionalArgc; ++i) {
        argv[argc++] = additionalArgv[i];
    }

    strcpy(tmpFilename, "tmp.");
    strcat(tmpFilename, ext);
    argv[argc++] = tmpFilename;

#ifdef DEBUG_TEST_IMAGES
    clContext * C = clContextCreate(NULL);
#else
    clContext * C = clContextCreate(&silentSystem);
#endif
    TEST_ASSERT_NOT_NULL(C);

    TEST_ASSERT_TRUE_MESSAGE(clContextParseArgs(C, argc, (const char **)argv), "failed to parse cmdline");

    clProfilePrimaries primaries;
    primaries.red[0] = C->params.primaries[0];
    primaries.red[1] = C->params.primaries[1];
    primaries.green[0] = C->params.primaries[2];
    primaries.green[1] = C->params.primaries[3];
    primaries.blue[0] = C->params.primaries[4];
    primaries.blue[1] = C->params.primaries[5];
    primaries.white[0] = C->params.primaries[6];
    primaries.white[1] = C->params.primaries[7];

    clProfileCurve curve;
    curve.type = C->params.curveType;
    curve.gamma = C->params.gamma;
    curve.implicitScale = 1.0f;

    clProfile * srcProfile = clProfileCreate(C, &primaries, &curve, C->params.luminance, NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(srcProfile, "Failed to create profile");

    clImage * srcImage = clImageParseString(C, C->inputFilename, C->params.bpc, srcProfile);

    clImage * dstImage = clContextRead(C, tmpFilename, NULL, NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(dstImage, "failed to read back image");

#ifdef DEBUG_TEST_IMAGES
    clProfileDebugDump(C, srcImage->profile, clTrue, 0);
    clProfileDebugDump(C, dstImage->profile, clTrue, 0);
    clProfileWrite(C, srcImage->profile, "src.icc");
    clProfileWrite(C, dstImage->profile, "dst.icc");
#endif

    TEST_ASSERT_TRUE_MESSAGE(clProfileComponentsMatch(C, srcImage->profile, dstImage->profile), "profiles don't match");

    clImageDiff * diff = clImageDiffCreate(C, srcImage, dstImage, 0.1f, threshold);
    TEST_ASSERT_NOT_NULL_MESSAGE(diff, "failed to diff images");
    if (diff->overThresholdCount > 0) {
        printf("ERROR: overThresholdCount: %d / %d, largestChannelDiff: %d\n", diff->overThresholdCount, diff->pixelCount, diff->largestChannelDiff);

        clWriteParams diffWriteParams;
        clWriteParamsSetDefaults(C, &diffWriteParams);
        clContextWrite(C, diff->image, "diff.png", NULL, &diffWriteParams);
        TEST_ASSERT_TRUE_MESSAGE(clFalse, "images don't match enough");
    }
    clImageDiffDestroy(C, diff);

    clProfileDestroy(C, srcProfile);
    clImageDestroy(C, srcImage);
    clImageDestroy(C, dstImage);

    clContextDestroy(C);
}

static void test_ext(struct clExtInfo * extInfo)
{
    for (char ** bpc = extInfo->bpc; *bpc; ++bpc) {
        for (char ** quality = extInfo->quality; *quality; ++quality) {
            for (char ** primaries = extInfo->primaries; *primaries; ++primaries) {
                for (char ** curve = extInfo->curve; *curve; ++curve) {
                    for (char ** luminance = extInfo->luminance; *luminance; ++luminance) {
                        for (char ** yuv = extInfo->yuv; *yuv; ++yuv) {
                            int argc = 0;
                            char * argv[32];

                            argv[argc++] = "-b";
                            argv[argc++] = *bpc;
                            argv[argc++] = "-p";
                            argv[argc++] = *primaries;
                            argv[argc++] = "-g";
                            argv[argc++] = *curve;
                            argv[argc++] = "-l";
                            argv[argc++] = *luminance;
                            argv[argc++] = "--yuv";
                            argv[argc++] = *yuv;

                            if (!strcmp(extInfo->ext, "jp2")) {
                                // Sad hacks for now, jp2 quality isn't any good, -r is much better
                                argv[argc++] = "-q";
                                argv[argc++] = "100";
                                argv[argc++] = "-r";
                                argv[argc++] = *quality;
                            } else {
                                argv[argc++] = "-q";
                                argv[argc++] = *quality;
                            }

                            int threshold = extInfo->threshold;
                            if ((threshold > 0) && !strcmp(*quality, "100")) {
                                threshold = 1; // rein in the threshold on lossless
                            }

                            printf("Testing ext:%s threshold:%d bpc:%s q/r:%s primaries:%s curve:%s luminance:%s yuv:%s\n",
                                   extInfo->ext,
                                   threshold,
                                   *bpc,
                                   *quality,
                                   *primaries,
                                   *curve,
                                   *luminance,
                                   *yuv);

                            test_images(extInfo->ext, threshold, argc, argv);
                        }
                    }
                }
            }
        }
    }
}

static void test_avif(void)
{
    struct clExtInfo extInfo = { "avif",
                                 6,
                                 { "8", "10", NULL },
                                 { "100", "90", NULL },
                                 { "bt709", "bt2020", NULL },
                                 { "2.2", "pq", NULL },
                                 { "un", "10000", NULL },
                                 { "420", "422", "444", NULL } };
    test_ext(&extInfo);
}

static void test_bmp(void)
{
    struct clExtInfo extInfo = { "bmp",
                                 0,
                                 { "8", "10", NULL },
                                 { "100", NULL },
                                 { "bt709", "bt2020", NULL },
                                 { "2.2", "pq", NULL },
                                 { "un", "300", "10000", NULL },
                                 { "420", NULL } };
    test_ext(&extInfo);
}

static void test_jpg(void)
{
    struct clExtInfo extInfo = { "jpg",
                                 3,
                                 { "8", NULL },
                                 { "100", "90", NULL },
                                 { "bt709", "bt2020", NULL },
                                 { "2.2", "pq", NULL },
                                 { "un", "300", "10000", NULL },
                                 { "420", NULL } };
    test_ext(&extInfo);
}

static void test_jp2(void)
{
    struct clExtInfo extInfo = { "jp2",
                                 6,
                                 { "8", "10", "12", "16", NULL },
                                 { "0", "20", NULL }, // these are actually rates
                                 { "bt709", "bt2020", NULL },
                                 { "2.2", "pq", NULL },
                                 { "un", "300", "10000", NULL },
                                 { "420", NULL } };
    test_ext(&extInfo);
}

static void test_png(void)
{
    struct clExtInfo extInfo = { "png",
                                 0,
                                 { "8", "16", NULL },
                                 { "100", NULL },
                                 { "bt709", "bt2020", NULL },
                                 { "2.2", "pq", NULL },
                                 { "un", "300", "10000", NULL },
                                 { "420", NULL } };
    test_ext(&extInfo);
}

static void test_tif(void)
{
    struct clExtInfo extInfo = { "tif",
                                 0,
                                 { "8", "16", NULL },
                                 { "100", NULL },
                                 { "bt709", "bt2020", NULL },
                                 { "2.2", "pq", NULL },
                                 { "un", "300", "10000", NULL },
                                 { "420", NULL } };
    test_ext(&extInfo);
}

static void test_webp(void)
{
    struct clExtInfo extInfo = {
        "webp",
        128, // webp YUV420 seams are awful
        { "8", NULL }, { "100", "90", NULL }, { "bt709", "bt2020", NULL }, { "2.2", NULL }, { "un", "300", NULL }, { "420", NULL }
    };
    test_ext(&extInfo);
}

int test_io(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_avif);
    RUN_TEST(test_bmp);
    RUN_TEST(test_jpg);
    RUN_TEST(test_jp2);
    RUN_TEST(test_png);
    RUN_TEST(test_tif);
    RUN_TEST(test_webp);

    return UNITY_END();
}
