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
#include <unity.h>

// --------------------------------------------------------------------------------------
// Helpers

// static void setFloat4(float c[4], float v0, float v1, float v2, float v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
// static void setFloat3(float c[3], float v0, float v1, float v2) { c[0] = v0; c[1] = v1; c[2] = v2; }
// static void setRGBA8_4(uint8_t c[4], uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
// static void setRGBA8_3(uint8_t c[3], uint8_t v0, uint8_t v1, uint8_t v2) { c[0] = v0; c[1] = v1; c[2] = v2; }
// static void setRGBA16_4(uint16_t c[4], uint16_t v0, uint16_t v1, uint16_t v2, uint16_t v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
// static void setRGBA16_3(uint16_t c[3], uint16_t v0, uint16_t v1, uint16_t v2) { c[0] = v0; c[1] = v1; c[2] = v2; }

static clContextSystem silentSystem;
static void clContextSilentLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(section);
    COLORIST_UNUSED(indent);
    COLORIST_UNUSED(format);
    COLORIST_UNUSED(args);
}
static void clContextSilentLogError(clContext * C, const char * format, va_list args)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(format);
    COLORIST_UNUSED(args);
}

// --------------------------------------------------------------------------------------
// Unity stubs

void setUp(void) {}
void tearDown(void) {}
void suiteSetUp(void) {}
int suiteTearDown(int num_failures) { return num_failures; }

// --------------------------------------------------------------------------------------
// Tests

static void test_clContext(void)
{
    clContext * C = clContextCreate(NULL);
    TEST_ASSERT_NOT_NULL(C);

    clContextDestroy(C);
}

static void test_clContextLog(void)
{
    clContext * C = clContextCreate(NULL);
    TEST_ASSERT_NOT_NULL(C);
    clContextLog(C, "unittest", 0, "testing clContextLog");
    clContextLog(C, "", 0, "testing clContextLog");
    clContextLog(C, "unittestunittest", 0, "testing clContextLog");
    clContextLog(C, "unittest", -3, "testing clContextLog");
    clContextLogError(C, "testing clContextLogError");

    clContextDestroy(C);
}

static void test_clAction(void)
{
    clContext * C = clContextCreate(&silentSystem);
    TEST_ASSERT_NOT_NULL(C);

    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "identify"), CL_ACTION_IDENTIFY);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "id"), CL_ACTION_IDENTIFY);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "generate"), CL_ACTION_GENERATE);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "gen"), CL_ACTION_GENERATE);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "calc"), CL_ACTION_CALC);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "convert"), CL_ACTION_CONVERT);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "modify"), CL_ACTION_MODIFY);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "report"), CL_ACTION_REPORT);
    TEST_ASSERT_EQUAL_INT(clActionFromString(C, "derp"), CL_ACTION_ERROR);

    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_NONE), "--");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_IDENTIFY), "identify");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_GENERATE), "generate");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_CALC), "calc");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_CONVERT), "convert");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_MODIFY), "modify");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_REPORT), "report");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, CL_ACTION_ERROR), "unknown");
    TEST_ASSERT_EQUAL_STRING(clActionToString(C, (clAction)555), "unknown");

    clContextDestroy(C);
}

static void test_clFormat(void)
{
    clContext * C = clContextCreate(&silentSystem);
    TEST_ASSERT_NOT_NULL(C);

    TEST_ASSERT_NULL(clContextFindFormat(C, NULL));
    TEST_ASSERT_NULL(clFormatDetect(C, "file_with_no_extension"));
    TEST_ASSERT_NULL(clFormatDetect(C, "not_an_image.txt"));
    TEST_ASSERT_EQUAL_STRING(clFormatDetect(C, "file.icc"), "icc");
    TEST_ASSERT_EQUAL_STRING(clFormatDetect(C, "file.png"), "png");

    TEST_ASSERT_EQUAL_INT(clFormatMaxDepth(C, "txt"), 8); // this will error, but return 8
    TEST_ASSERT_EQUAL_INT(clFormatMaxDepth(C, "jpg"), 8);
    TEST_ASSERT_EQUAL_INT(clFormatMaxDepth(C, "bmp"), 10);
    TEST_ASSERT_EQUAL_INT(clFormatMaxDepth(C, "png"), 16);

    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "txt", 8), 8); // this will error, but return 8
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "jpg", 8), 8);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "jpg", 6), 8);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "bmp", 8), 8);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "bmp", 10), 10);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "png", 8), 8);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "png", 12), 16);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "png", 16), 16);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "jp2", 8), 8);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "jp2", 12), 12);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "jp2", 16), 16);
    TEST_ASSERT_EQUAL_INT(clFormatBestDepth(C, "jp2", 20), 16);

    TEST_ASSERT_TRUE(clFormatExists(C, "png"));
    TEST_ASSERT_FALSE(clFormatExists(C, "txt"));

    clContextDestroy(C);
}

static void test_clTonemap(void)
{
    clContext * C = clContextCreate(&silentSystem);
    TEST_ASSERT_NOT_NULL(C);

    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "on"), CL_TONEMAP_ON);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "yes"), CL_TONEMAP_ON);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "enabled"), CL_TONEMAP_ON);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "off"), CL_TONEMAP_OFF);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "no"), CL_TONEMAP_OFF);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "disabled"), CL_TONEMAP_OFF);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "auto"), CL_TONEMAP_AUTO);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "automatic"), CL_TONEMAP_AUTO);
    TEST_ASSERT_EQUAL_INT(clTonemapFromString(C, "derp"), CL_TONEMAP_AUTO); // This is weird/dumb maybe, but auto IS the fallback

    TEST_ASSERT_EQUAL_STRING(clTonemapToString(C, CL_TONEMAP_AUTO), "auto");
    TEST_ASSERT_EQUAL_STRING(clTonemapToString(C, CL_TONEMAP_ON), "on");
    TEST_ASSERT_EQUAL_STRING(clTonemapToString(C, CL_TONEMAP_OFF), "off");
    TEST_ASSERT_EQUAL_STRING(clTonemapToString(C, (clTonemap)555), "unknown");

    clContextDestroy(C);
}

static void test_clFilter(void)
{
    clContext * C = clContextCreate(&silentSystem);
    TEST_ASSERT_NOT_NULL(C);

    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "auto"), CL_FILTER_AUTO);
    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "box"), CL_FILTER_BOX);
    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "triangle"), CL_FILTER_TRIANGLE);
    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "cubic"), CL_FILTER_CUBICBSPLINE);
    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "catmullrom"), CL_FILTER_CATMULLROM);
    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "mitchell"), CL_FILTER_MITCHELL);
    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "nearest"), CL_FILTER_NEAREST);
    TEST_ASSERT_EQUAL_INT(clFilterFromString(C, "derp"), CL_FILTER_INVALID);

    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_AUTO), "auto");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_BOX), "box");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_TRIANGLE), "triangle");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_CUBICBSPLINE), "cubic");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_CATMULLROM), "catmullrom");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_MITCHELL), "mitchell");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_NEAREST), "nearest");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, CL_FILTER_INVALID), "invalid");
    TEST_ASSERT_EQUAL_STRING(clFilterToString(C, (clFilter)555), "invalid");

    clContextDestroy(C);
}

static void test_stockPrimaries(void)
{
    clContext * C = clContextCreate(&silentSystem);
    TEST_ASSERT_NOT_NULL(C);

    float bt709[8] = { 0.64f, 0.33f, 0.30f, 0.60f, 0.15f, 0.06f, 0.3127f, 0.3290f };
    float bt2020[8] = { 0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f };
    float p3[8] = { 0.68f, 0.32f, 0.265f, 0.690f, 0.150f, 0.060f, 0.3127f, 0.3290f };

    clProfilePrimaries primaries;
    float rawPrimaries[8];

    TEST_ASSERT_TRUE(clContextGetStockPrimaries(C, "bt709", &primaries));
    memcpy(rawPrimaries, &primaries, sizeof(rawPrimaries));
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(rawPrimaries, bt709, 8);
    TEST_ASSERT_TRUE(clContextGetStockPrimaries(C, "p3", &primaries));
    memcpy(rawPrimaries, &primaries, sizeof(rawPrimaries));
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(rawPrimaries, p3, 8);
    TEST_ASSERT_TRUE(clContextGetStockPrimaries(C, "bt2020", &primaries));
    memcpy(rawPrimaries, &primaries, sizeof(rawPrimaries));
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(rawPrimaries, bt2020, 8);
    TEST_ASSERT_FALSE(clContextGetStockPrimaries(C, "derp", &primaries));

    TEST_ASSERT_TRUE(clContextGetRawStockPrimaries(C, "bt709", rawPrimaries));
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(rawPrimaries, bt709, 8);
    TEST_ASSERT_TRUE(clContextGetRawStockPrimaries(C, "p3", rawPrimaries));
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(rawPrimaries, p3, 8);
    TEST_ASSERT_TRUE(clContextGetRawStockPrimaries(C, "bt2020", rawPrimaries));
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(rawPrimaries, bt2020, 8);
    TEST_ASSERT_FALSE(clContextGetRawStockPrimaries(C, "derp", rawPrimaries));

    clContextDestroy(C);
}

#define ARGS(A) (sizeof(A) / sizeof(A[0])), A

static void test_clContextParseArgs(void)
{
    clContext * C = clContextCreate(&silentSystem);
    TEST_ASSERT_NOT_NULL(C);

    {
        const char * argv[] = { "colorist", "identify", "image.png" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
        TEST_ASSERT_EQUAL_INT(C->action, CL_ACTION_IDENTIFY);
        TEST_ASSERT_EQUAL_STRING(C->inputFilename, "image.png");
    }

    {
        // stock primaries
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-p", "bt709" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // custom primaries
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-p", "0.64,0.33,0.30,0.60,0.15,0.06,0.3127,0.329" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // too many primaries
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-p", "0.64,0.33,0.30,0.60,0.15,0.06,0.3127,0.329,0.555" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // too few primaries
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-p", "0.64,0.33,0.30,0.60,0.15,0.06,0.3127" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // rect
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-a", "-z", "0,0,1,1" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // rect: too many
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-z", "0,0,1,1,1" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // rect: too few
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-z", "0,0,1" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // tonemap
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "--tonemap", "on" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        const char * filterNames[] = {
            "auto",
            "box",
            "triangle",
            "cubic",
            "catmullrom",
            "mitchell",
            "nearest"
        };
        const int filterNamesCount = sizeof(filterNames) / sizeof(filterNames[0]);
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "--resize", NULL };
        for (int i = 0; i < filterNamesCount; ++i) {
            char description[128];
            char buffer[128];
            sprintf(buffer, "5,5,%s", filterNames[i]);
            sprintf(description, "Resize with %s", buffer);
            argv[5] = buffer;
            TEST_ASSERT_TRUE_MESSAGE(clContextParseArgs(C, ARGS(argv)), description);
        }
    }

    {
        // resize: too many params
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-r", "5,5,5,5" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // resize: unrecognized filter
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-r", "5,5,derp" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // resize: nonzero dimension required
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-r", "0,0" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // -r requires an argument
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-r" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // coverage kitchen sink
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-a", "-b", "16", "-c", "copyright",
                                "-d", "description", "-f", "png", "-g", "2.2", "-g", "s", "-h", "--hald", "hald.png",
                                "--iccin", "iccin.icc", "-j", "4", "-j", "0", "--json", "-l", "1000", "-l", "s",
                                "--iccout", "iccout.icc", "-q", "50", "--striptags", "lumi", "-t", "on", "-v",
                                "--cmm", "lcms", "--cmm", "ccmm", "--rect", "0,0,1,1", "--crop", "0,0,1,1",
                                "-2", "50", "--jp2rate", "50", "--rate", "50" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // invalid bpp
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-b", "foo" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // test everything that requires an argument
        const char * needsArgs[] = { "-b", "-c", "-d", "-f", "-g", "--hald", "--iccin", "-j", "-l",
                                     "--iccout", "-p", "-q", "--striptags", "-t", "--cms", "--crop", "--rate" };
        const int needsArgsCount = sizeof(needsArgs) / sizeof(needsArgs[0]);
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", NULL };
        for (int i = 0; i < needsArgsCount; ++i) {
            argv[4] = needsArgs[i];
            TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
        }
    }

    {
        // jobs clamping
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-j", "1000" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // jobs clamping, part 2
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-j", "0" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // unknown format
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "-f", "txt" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // unknown CMM
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "--cmm", "derp" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // unknown parameter
        const char * argv[] = { "colorist", "convert", "input.png", "output.png", "--derp" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // unknown action
        const char * argv[] = { "colorist", "derp", "input.png" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // too many positional arguments
        const char * argv[] = { "colorist", "convert", "a", "b", "c" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // identify requires an input name
        const char * argv[] = { "colorist", "identify" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // identify does not accept an output filename
        const char * argv[] = { "colorist", "identify", "a.png", "b.png" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // calc
        const char * argv[] = { "colorist", "calc", "#ff0000" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // calc requires an input string
        const char * argv[] = { "colorist", "calc" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // generate with input and output strings
        const char * argv[] = { "colorist", "generate", "#ff00000", "out.png" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // generate with only an output string
        const char * argv[] = { "colorist", "generate", "foo.icc" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // generate requires an output filename
        const char * argv[] = { "colorist", "generate" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // convert requires an input filename
        const char * argv[] = { "colorist", "convert" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // convert requires an output filename
        const char * argv[] = { "colorist", "convert", "input.png" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // modify requires an input filename
        const char * argv[] = { "colorist", "modify" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // modify requires an output filename
        const char * argv[] = { "colorist", "modify", "input.png" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        const char * argv[] = { "colorist", "modify", "input.png", "output.png" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // report requires an input filename
        const char * argv[] = { "colorist", "report" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        // report requires an output filename
        const char * argv[] = { "colorist", "report", "input.png" };
        TEST_ASSERT_FALSE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        const char * argv[] = { "colorist", "report", "input.png", "output.html" };
        TEST_ASSERT_TRUE(clContextParseArgs(C, ARGS(argv)));
    }

    {
        C->params.autoGrade = clTrue;
        C->params.bpp = 16;
        C->params.copyright = NULL;
        C->params.description = NULL;
        C->params.formatName = NULL;
        C->params.gamma = -1.0f;
        C->params.hald = NULL;
        C->help = clFalse;
        C->verbose = clFalse;
        C->ccmmAllowed = clFalse;
        C->iccOverrideIn = NULL;
        C->params.iccOverrideOut = NULL;
        C->params.luminance = -1;
        clContextPrintArgs(C);
        C->params.autoGrade = clFalse;
        C->params.bpp = 0;
        C->params.copyright = "copyright";
        C->params.description = "description";
        C->params.formatName = "png";
        C->params.gamma = 2.2f;
        C->params.hald = "hald.png";
        C->help = clTrue;
        C->verbose = clTrue;
        C->ccmmAllowed = clTrue;
        C->iccOverrideIn = "iccin.png";
        C->params.iccOverrideOut = "iccout.png";
        C->params.luminance = 0;
        C->params.stripTags = NULL;
        clContextPrintArgs(C);
        C->params.gamma = 0.0f;
        C->params.luminance = 300;
        C->params.primaries[0] = 1.0f;
        C->params.stripTags = "lumi";
        C->inputFilename = NULL;
        C->outputFilename = NULL;
        clContextPrintArgs(C);
    }

    clContextPrintSyntax(C);
    clContextDestroy(C);
}

// --------------------------------------------------------------------------------------
// Main / List of active tests

int main(int argc, char * argv[])
{
    COLORIST_UNUSED(argc);
    COLORIST_UNUSED(argv);

    // Lots of these tests will be purposefully spewing errors or requesting conversions,
    // there's no need to muddy stdout with colorist yapping away on most of them.
    silentSystem.alloc = clContextDefaultAlloc;
    silentSystem.free = clContextDefaultFree;
    silentSystem.log = clContextSilentLog;
    silentSystem.error = clContextSilentLogError;

    UNITY_BEGIN();

    RUN_TEST(test_clContext);
    RUN_TEST(test_clContextLog);
    RUN_TEST(test_clAction);
    RUN_TEST(test_clFormat);
    RUN_TEST(test_clTonemap);
    RUN_TEST(test_clFilter);
    RUN_TEST(test_stockPrimaries);
    RUN_TEST(test_clContextParseArgs);

    return UNITY_END();
}
