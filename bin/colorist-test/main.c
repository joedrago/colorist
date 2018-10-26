// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include "colorist/transform.h"

static void setFloat4(float c[4], float v0, float v1, float v2, float v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
static void setFloat3(float c[3], float v0, float v1, float v2) { c[0] = v0; c[1] = v1; c[2] = v2; }
static void setRGBA8_4(uint8_t c[4], uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
static void setRGBA8_3(uint8_t c[3], uint8_t v0, uint8_t v1, uint8_t v2) { c[0] = v0; c[1] = v1; c[2] = v2; }
static void setRGBA16_4(uint16_t c[4], uint16_t v0, uint16_t v1, uint16_t v2, uint16_t v3) { c[0] = v0; c[1] = v1; c[2] = v2; c[3] = v3; }
static void setRGBA16_3(uint16_t c[3], uint16_t v0, uint16_t v1, uint16_t v2) { c[0] = v0; c[1] = v1; c[2] = v2; }

int main(int argc, char * argv[])
{
    clContext * C = clContextCreate(NULL);
    clConversionParams params;
    clProfilePrimaries primaries;
    clProfileCurve curve;
    clTransform * transform;
    struct clProfile * srcProfile;
    struct clProfile * dstProfile;
    clImage * srcImage;
    clImage * dstImage;

#if defined(DEBUG_MATRIX_MATH)
    {
        clProfile * bt709;
        clProfile * bt2020;
        C = clContextCreate(NULL);

        clContextGetStockPrimaries(C, "bt709", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 1.0f;
        bt709 = clProfileCreate(C, &primaries, &curve, 0, NULL);

        clContextGetStockPrimaries(C, "bt2020", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 1.0f;
        bt2020 = clProfileCreate(C, &primaries, &curve, 0, NULL);

        transform = clTransformCreate(C, bt709, CL_XF_RGBA, 32, bt2020, CL_XF_RGBA, 32);
        clTransformPrepare(C, transform);
        clTransformDestroy(C, transform);

        clContextDestroy(C);
        return 0;
    }
#endif

    // Basic clImageDebugDump test
    {
        C = clContextCreate(NULL);

        srcImage = clImageParseString(C, "8x8,(255,0,0)", 8, NULL);
        clImageDebugDump(C, srcImage, 0, 0, 1, 1, 0);

        clImageDestroy(C, srcImage);
        clContextDestroy(C);
    }

    // Test all CCMM RGBA reformat paths
    {
        C = clContextCreate(NULL);
        clConversionParamsSetDefaults(C, &params);
        params.jobs = 1;

        // RGBA8 -> RGBA8
        srcImage = clImageParseString(C, "8x8,(255,0,0)", 8, NULL);
        dstImage = clImageConvert(C, srcImage, &params, NULL);
        clImageDestroy(C, srcImage);
        clImageDestroy(C, dstImage);

        // RGBA16 -> RGBA16
        srcImage = clImageParseString(C, "8x8,(255,0,0)", 16, NULL);
        dstImage = clImageConvert(C, srcImage, &params, NULL);
        clImageDestroy(C, srcImage);
        clImageDestroy(C, dstImage);

        // RGBA16 -> RGBA8
        params.bpp = 8;
        srcImage = clImageParseString(C, "8x8,(255,0,0)", 16, NULL);
        dstImage = clImageConvert(C, srcImage, &params, NULL);
        clImageDestroy(C, srcImage);
        clImageDestroy(C, dstImage);

        // RGBA8 -> RGBA16
        params.bpp = 16;
        srcImage = clImageParseString(C, "8x8,(255,0,0)", 8, NULL);
        dstImage = clImageConvert(C, srcImage, &params, NULL);
        clImageDestroy(C, srcImage);
        clImageDestroy(C, dstImage);

        clContextDestroy(C);
    }

    // Directly test RGB(A) -> RGB(A) reformats
    {
        uint8_t srcRGBA8[4];
        uint8_t dstRGBA8[4];
        uint16_t srcRGBA16[4];
        uint16_t dstRGBA16[4];
        C = clContextCreate(NULL);
        srcProfile = clProfileCreateStock(C, CL_PS_SRGB);

        // RGB8 -> RGBA8
        setRGBA8_4(srcRGBA8, 255, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 8, srcProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA8, 1);
        printf("RGB8(%u, %u, %u) -> RGBA8(%u, %u, %u, %u)\n", srcRGBA8[0], srcRGBA8[1], srcRGBA8[2], dstRGBA8[0], dstRGBA8[1], dstRGBA8[2], dstRGBA8[3]);
        clTransformDestroy(C, transform);

        // RGBA8 -> RGB8
        setRGBA8_4(srcRGBA8, 255, 0, 0, 255);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 8, srcProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA8, 1);
        printf("RGB8(%u, %u, %u, %u) -> RGBA8(%u, %u, %u)\n", srcRGBA8[0], srcRGBA8[1], srcRGBA8[2], srcRGBA8[3], dstRGBA8[0], dstRGBA8[1], dstRGBA8[2]);
        clTransformDestroy(C, transform);

        // RGB16 -> RGBA16
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 16, srcProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA16, 1);
        printf("RGB16(%u, %u, %u) -> RGBA16(%u, %u, %u, %u)\n", srcRGBA16[0], srcRGBA16[1], srcRGBA16[2], dstRGBA16[0], dstRGBA16[1], dstRGBA16[2], dstRGBA16[3]);
        clTransformDestroy(C, transform);

        // RGBA16 -> RGB16
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 65535);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 16, srcProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA16, 1);
        printf("RGB16(%u, %u, %u, %u) -> RGBA16(%u, %u, %u)\n", srcRGBA16[0], srcRGBA16[1], srcRGBA16[2], srcRGBA16[3], dstRGBA16[0], dstRGBA16[1], dstRGBA16[2]);
        clTransformDestroy(C, transform);

        // RGB8 -> RGBA16
        setRGBA8_4(srcRGBA8, 255, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 8, srcProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA16, 1);
        printf("RGB8(%u, %u, %u) -> RGBA16(%u, %u, %u, %u)\n", srcRGBA8[0], srcRGBA8[1], srcRGBA8[2], dstRGBA16[0], dstRGBA16[1], dstRGBA16[2], dstRGBA16[3]);
        clTransformDestroy(C, transform);

        // RGB16 -> RGBA8
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 16, srcProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA8, 1);
        printf("RGB16(%u, %u, %u) -> RGBA8(%u, %u, %u, %u)\n", srcRGBA16[0], srcRGBA16[1], srcRGBA16[2], dstRGBA8[0], dstRGBA8[1], dstRGBA8[2], dstRGBA8[3]);
        clTransformDestroy(C, transform);

        clProfileDestroy(C, srcProfile);
        clContextDestroy(C);
    }

    // Directly test RGB(A) -> RGB(A) transforms
    {
        uint8_t srcRGBA8[4];
        uint8_t dstRGBA8[4];
        uint16_t srcRGBA16[4];
        uint16_t dstRGBA16[4];
        C = clContextCreate(NULL);
        srcProfile = clProfileCreateStock(C, CL_PS_SRGB);
        clContextGetStockPrimaries(C, "bt2020", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 2.2f;
        dstProfile = clProfileCreate(C, &primaries, &curve, 10000, NULL);

        // RGB8 -> RGBA8
        setRGBA8_4(srcRGBA8, 255, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 8, dstProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA8, 1);
        clTransformDestroy(C, transform);

        // RGBA8 -> RGBA8
        setRGBA8_4(srcRGBA8, 255, 0, 0, 255);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 8, dstProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA8, 1);
        clTransformDestroy(C, transform);

        // RGBA8 -> RGB8
        setRGBA8_4(srcRGBA8, 255, 0, 0, 255);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 8, dstProfile, CL_XF_RGB, 8);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA8, 1);
        clTransformDestroy(C, transform);

        // RGB16 -> RGBA16
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 16, dstProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA16, 1);
        clTransformDestroy(C, transform);

        // RGBA16 -> RGB16
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 65535);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 16, dstProfile, CL_XF_RGB, 16);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA16, 1);
        clTransformDestroy(C, transform);

        // RGBA16 -> RGBA16
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 65535);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 16, dstProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA16, 1);
        clTransformDestroy(C, transform);

        // RGBA16 -> RGBA8
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 65535);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 16, dstProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA8, 1);
        clTransformDestroy(C, transform);

        // RGBA8 -> RGBA16
        setRGBA8_4(srcRGBA8, 255, 0, 0, 255);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 8, dstProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA16, 1);
        clTransformDestroy(C, transform);

        // RGBA16 -> RGB8
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 65535);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 16, dstProfile, CL_XF_RGB, 8);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA8, 1);
        clTransformDestroy(C, transform);

        // RGB8 -> RGBA16
        setRGBA8_4(srcRGBA8, 255, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 8, dstProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA16, 1);
        clTransformDestroy(C, transform);

        // RGBA8 -> RGB16
        setRGBA8_4(srcRGBA8, 255, 0, 0, 255);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 8, dstProfile, CL_XF_RGB, 16);
        clTransformRun(C, transform, 1, srcRGBA8, dstRGBA16, 1);
        clTransformDestroy(C, transform);

        // RGB16 -> RGBA8
        setRGBA16_4(srcRGBA16, 65535, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 16, dstProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, srcRGBA16, dstRGBA8, 1);
        printf("RGB16(%u, %u, %u) -> RGBA8(%u, %u, %u, %u)\n", srcRGBA16[0], srcRGBA16[1], srcRGBA16[2], dstRGBA8[0], dstRGBA8[1], dstRGBA8[2], dstRGBA8[3]);
        clTransformDestroy(C, transform);

        clProfileDestroy(C, srcProfile);
        clProfileDestroy(C, dstProfile);
        clContextDestroy(C);
    }

    // Directly test RGB float to 8/16 reformatting (and back)
    {
        float rgba[4];
        uint8_t rgba8[4];
        uint16_t rgba16[4];
        C = clContextCreate(NULL);
        srcProfile = clProfileCreateStock(C, CL_PS_SRGB);

        setRGBA8_4(rgba8, 255, 0, 0, 255);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 8, srcProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba8, rgba, 1);
        clTransformDestroy(C, transform);

        setRGBA8_4(rgba8, 255, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 8, srcProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba8, rgba, 1);
        clTransformDestroy(C, transform);

        setRGBA16_4(rgba16, 65535, 0, 0, 65535);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 16, srcProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba16, rgba, 1);
        clTransformDestroy(C, transform);

        setRGBA16_4(rgba16, 65535, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 16, srcProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba16, rgba, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 1.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, srcProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, rgba, rgba8, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 0.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 32, srcProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, rgba, rgba8, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 1.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, srcProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, rgba, rgba16, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 0.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 32, srcProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, rgba, rgba16, 1);
        clTransformDestroy(C, transform);

        clContextDestroy(C);
    }

    // Directly test RGB float to 8/16 transforms (and back)
    {
        float rgba[4];
        uint8_t rgba8[4];
        uint16_t rgba16[4];
        C = clContextCreate(NULL);
        srcProfile = clProfileCreateStock(C, CL_PS_SRGB);
        clContextGetStockPrimaries(C, "bt2020", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 2.2f;
        dstProfile = clProfileCreate(C, &primaries, &curve, 10000, NULL);

        setRGBA8_4(rgba8, 255, 0, 0, 255);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 8, dstProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba8, rgba, 1);
        clTransformDestroy(C, transform);

        setRGBA8_4(rgba8, 255, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 8, dstProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba8, rgba, 1);
        clTransformDestroy(C, transform);

        setRGBA16_4(rgba16, 65535, 0, 0, 65535);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 16, dstProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba16, rgba, 1);
        clTransformDestroy(C, transform);

        setRGBA16_4(rgba16, 65535, 0, 0, 0);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 16, dstProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, rgba16, rgba, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 1.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, dstProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, rgba, rgba8, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 0.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 32, dstProfile, CL_XF_RGBA, 8);
        clTransformRun(C, transform, 1, rgba, rgba8, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 1.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, dstProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, rgba, rgba16, 1);
        clTransformDestroy(C, transform);

        setFloat4(rgba, 1.0f, 0.0f, 0.0f, 0.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 32, dstProfile, CL_XF_RGBA, 16);
        clTransformRun(C, transform, 1, rgba, rgba16, 1);
        clTransformDestroy(C, transform);

        clProfileDestroy(C, srcProfile);
        clProfileDestroy(C, dstProfile);
        clContextDestroy(C);
    }

    // Directly test floating point transforms
    {
        float srcRGBA[4];
        float dstRGBA[4];
        float xyy[3];
        float xyz[3];
        C = clContextCreate(NULL);
        srcProfile = clProfileCreateStock(C, CL_PS_SRGB);
        clContextGetStockPrimaries(C, "bt2020", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 2.2f;
        dstProfile = clProfileCreate(C, &primaries, &curve, 10000, NULL);

        // as close to a noop as float->float gets
        setFloat4(srcRGBA, 1.0f, 0.0f, 0.0f, 1.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, srcProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, srcRGBA, dstRGBA, 1);
        printf("sRGBA(%g,%g,%g,%g) -> sRGBA(%g,%g,%g,%g)\n", srcRGBA[0], srcRGBA[1], srcRGBA[2], srcRGBA[3], dstRGBA[0], dstRGBA[1], dstRGBA[2], dstRGBA[3]);
        clTransformDestroy(C, transform);

        // sRGBA -> XYZ
        setFloat4(srcRGBA, 1.0f, 0.0f, 0.0f, 1.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, NULL, CL_XF_XYZ, 32);
        clTransformRun(C, transform, 1, srcRGBA, xyz, 1);
        printf("sRGBA(%g,%g,%g,%g) -> XYZ(%g,%g,%g)\n", srcRGBA[0], srcRGBA[1], srcRGBA[2], srcRGBA[3], xyz[0], xyz[1], xyz[2]);
        clTransformDestroy(C, transform);

        // XYZ -> BT.2020 10k nits G2.2
        setFloat3(xyy, primaries.white[0], primaries.white[1], 1.0f);
        clTransformXYYToXYZ(C, xyz, xyy);
        transform = clTransformCreate(C, NULL, CL_XF_XYZ, 32, dstProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, xyz, srcRGBA, 1);
        printf("xyY(%g,%g,%g) -> XYZ(%g,%g,%g) -> BT2020_10k_G22(%g,%g,%g,%g)\n", xyy[0], xyy[1], xyy[2], xyz[0], xyz[1], xyz[2], srcRGBA[0], srcRGBA[1], srcRGBA[2], srcRGBA[3]);
        clTransformDestroy(C, transform);

        // sRGBA -> BT.2020 10k nits G2.2
        setFloat4(srcRGBA, 1.0f, 0.0f, 0.0f, 1.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, dstProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, srcRGBA, dstRGBA, 1);
        printf("sRGBA(%g,%g,%g,%g) -> BT2020_10k_G22(%g,%g,%g,%g)\n", srcRGBA[0], srcRGBA[1], srcRGBA[2], srcRGBA[3], dstRGBA[0], dstRGBA[1], dstRGBA[2], dstRGBA[3]);
        clTransformDestroy(C, transform);

        // sRGBA -> sRGB
        setFloat4(srcRGBA, 1.0f, 0.0f, 0.0f, 1.0f);
        setFloat4(dstRGBA, 0.0f, 0.0f, 0.0f, 0.0f);
        transform = clTransformCreate(C, srcProfile, CL_XF_RGBA, 32, srcProfile, CL_XF_RGB, 32);
        clTransformRun(C, transform, 1, srcRGBA, dstRGBA, 1);
        printf("sRGBA(%g,%g,%g) -> sRGB(%g,%g,%g) (%g == 0)\n", srcRGBA[0], srcRGBA[1], srcRGBA[2], dstRGBA[0], dstRGBA[1], dstRGBA[2], dstRGBA[3]);
        clTransformDestroy(C, transform);

        // sRGB -> sRGBA
        setFloat4(srcRGBA, 1.0f, 0.0f, 0.0f, 0.0f); // set alpha to 0 to prove it doesn't carry over
        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 32, srcProfile, CL_XF_RGBA, 32);
        clTransformRun(C, transform, 1, srcRGBA, dstRGBA, 1);
        printf("sRGB(%g,%g,%g) -> sRGBA(%g,%g,%g,%g)\n", srcRGBA[0], srcRGBA[1], srcRGBA[2], dstRGBA[0], dstRGBA[1], dstRGBA[2], dstRGBA[3]);
        clTransformDestroy(C, transform);

        clProfileDestroy(C, srcProfile);
        clProfileDestroy(C, dstProfile);
        clContextDestroy(C);
    }

    // Compare CCMM and LittleCMS
    {
        float colors[][3] = {
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 1.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 1.0f },
            { 1.0f, 0.0f, 1.0f },
            { 1.0f, 0.5f, 0.0f },
            { 1.0f, 1.0f, 1.0f },
        };
        const int colorCount = sizeof(colors) / sizeof(colors[0]);
        int i;

        float srcRGB[3];
        float xyz[3];
        float xyy[3];
        C = clContextCreate(NULL);
        clContextGetStockPrimaries(C, "bt2020", &primaries);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 2.2f;
        srcProfile = clProfileCreate(C, &primaries, &curve, 10000, NULL);

        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 32, NULL, CL_XF_XYZ, 32);
        for (i = 0; i < colorCount; ++i) {
            setFloat3(srcRGB, colors[i][0], colors[i][1], colors[i][2]);
            printf("---\n");

            C->ccmmAllowed = clTrue;
            clTransformRun(C, transform, 1, srcRGB, xyz, 1);
            clTransformXYZToXYY(C, xyy, xyz, primaries.white[0], primaries.white[1]);
            printf("CCMM: RGB(%g,%g,%g) -> xyY(%g,%g,%g)\n", srcRGB[0], srcRGB[1], srcRGB[2], xyy[0], xyy[1], xyy[2]);

            C->ccmmAllowed = clFalse;
            clTransformRun(C, transform, 1, srcRGB, xyz, 1);
            clTransformXYZToXYY(C, xyy, xyz, primaries.white[0], primaries.white[1]);
            printf("LCMS: RGB(%g,%g,%g) -> xyY(%g,%g,%g)\n", srcRGB[0], srcRGB[1], srcRGB[2], xyy[0], xyy[1], xyy[2]);
        }
        clTransformDestroy(C, transform);

        clProfileDestroy(C, srcProfile);
        clContextDestroy(C);
    }

    // Compare CCMM and LittleCMS, continued
    {
        float colors[][3] = {
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 1.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 1.0f },
            { 1.0f, 0.0f, 1.0f },
            { 1.0f, 0.5f, 0.0f },
            { 1.0f, 1.0f, 1.0f },
        };
        const int colorCount = sizeof(colors) / sizeof(colors[0]);
        int i;

        uint16_t * srcU16;
        uint16_t * dstU16;
        C = clContextCreate(NULL);
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 2.2f;
        char imageString[512];

        clContextGetStockPrimaries(C, "bt709", &primaries);
        srcProfile = clProfileCreate(C, &primaries, &curve, 300, "sRGB");

        clContextGetStockPrimaries(C, "bt2020", &primaries);
        dstProfile = clProfileCreate(C, &primaries, &curve, 10000, "BT2020 10k G22");

        i = 0;
        sprintf(imageString, "f(%g,%g,%g)", colors[i][0], colors[i][1], colors[i][2]);
        srcImage = clImageParseString(C, imageString, 16, srcProfile);

        C->ccmmAllowed = clTrue;
        clConversionParamsSetDefaults(C, &params);
        dstImage = clImageConvert(C, srcImage, &params, dstProfile);
        srcU16 = (uint16_t *)srcImage->pixels;
        dstU16 = (uint16_t *)dstImage->pixels;
        printf("\n\nCCMM: bt709_300(%u,%u,%u) -> bt2020_10000(%u,%u,%u)\n", srcU16[0], srcU16[1], srcU16[2], dstU16[0], dstU16[1], dstU16[2]);
        clImageDebugDump(C, dstImage, 0, 0, 1, 1, 0);
        printf("\n");
        clImageDestroy(C, dstImage);

        C->ccmmAllowed = clFalse;
        clConversionParamsSetDefaults(C, &params);
        dstImage = clImageConvert(C, srcImage, &params, dstProfile);
        srcU16 = (uint16_t *)srcImage->pixels;
        dstU16 = (uint16_t *)dstImage->pixels;
        printf("\n\nLCMS: bt709_300(%u,%u,%u) -> bt2020_10000(%u,%u,%u)\n", srcU16[0], srcU16[1], srcU16[2], dstU16[0], dstU16[1], dstU16[2]);
        clImageDebugDump(C, dstImage, 0, 0, 1, 1, 0);
        printf("\n");
        clImageDestroy(C, dstImage);

        clProfileDestroy(C, srcProfile);
        clProfileDestroy(C, dstProfile);
        clContextDestroy(C);
    }

#if 0
    // Compare CCMM and LittleCMS, continued again
    {
        const float MIN_CMM_DIFFERENTIAL = 0.0001f;
        int r, g, b;
        float src[3];
        float dstCCMM[3];
        float dstLCMS[3];

        C = clContextCreate(NULL);

        curve.type = CL_PCT_GAMMA;
        curve.gamma = 2.2f;

        clContextGetStockPrimaries(C, "bt709", &primaries);
        srcProfile = clProfileCreate(C, &primaries, &curve, 300, "BT709 300 G22");
        // srcProfile = clProfileRead(C, "HDR_P3_D65_ST2084.icc");

        clContextGetStockPrimaries(C, "bt2020", &primaries);
        dstProfile = clProfileCreate(C, &primaries, &curve, 10000, "BT2020 10k G22");

        transform = clTransformCreate(C, srcProfile, CL_XF_RGB, 32, dstProfile, CL_XF_RGB, 32);

        for (r = 0; r < 256; ++r) {
            printf("R: %d\n", r);
            for (g = 0; g < 256; ++g) {
                for (b = 0; b < 256; ++b) {
                    setFloat3(src, r / 255.0f, g / 255.0f, b / 255.0f);

                    C->ccmmAllowed = clTrue;
                    clTransformRun(C, transform, 1, src, dstCCMM, 1);
                    C->ccmmAllowed = clFalse;
                    clTransformRun(C, transform, 1, src, dstLCMS, 1);

                    dstLCMS[0] /= 100.0f;
                    dstLCMS[1] /= 100.0f;
                    dstLCMS[2] /= 100.0f;

                    if ((fabsf(dstCCMM[0] - dstLCMS[0]) > MIN_CMM_DIFFERENTIAL)
                        || (fabsf(dstCCMM[1] - dstLCMS[1]) > MIN_CMM_DIFFERENTIAL)
                        || (fabsf(dstCCMM[2] - dstLCMS[2]) > MIN_CMM_DIFFERENTIAL))
                    {
                        printf("SRC(%g,%g,%g) CCMM(%g,%g,%g) LCMS(%g,%g,%g)\n",
                            src[0], src[1], src[2],
                            dstCCMM[0], dstCCMM[1], dstCCMM[2],
                            dstLCMS[0], dstLCMS[1], dstLCMS[2]);
                    }
                }
            }
        }

        clProfileDestroy(C, srcProfile);
        clProfileDestroy(C, dstProfile);
        clContextDestroy(C);
    }
#endif /* if 1 */

    printf("colorist-test Complete.\n");
    return 0;
}
