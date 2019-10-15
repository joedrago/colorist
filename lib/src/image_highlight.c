// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include <string.h>

static float calcOverbright(float Y, float overbrightScale, float maxY)
{
    // Even at 10,000 nits, this is only 1 nit difference. If its less than this, we're not over.
    static const float REASONABLY_OVERBRIGHT = 0.0001f;

    float p = Y / maxY;
    if (p > (1.0f + REASONABLY_OVERBRIGHT)) {
        p = (p - 1.0f) / (overbrightScale - 1.0f);
        p = CL_CLAMP(p, 0.0f, 1.0f);
        return p;
    }
    return 0.0f;
}

static void calcGamutDistances(float x, float y, const clProfilePrimaries * primaries, float outDistances[3])
{
    float rX = primaries->red[0];
    float rY = primaries->red[1];
    float gX = primaries->green[0];
    float gY = primaries->green[1];
    float bX = primaries->blue[0];
    float bY = primaries->blue[1];

    float distBetweenRG = sqrtf(((rY - gY) * (rY - gY)) + ((rX - gX) * (rX - gX)));
    float distBetweenGB = sqrtf(((gY - bY) * (gY - bY)) + ((gX - bX) * (gX - bX)));
    float distBetweenRB = sqrtf(((rY - bY) * (rY - bY)) + ((rX - bX) * (rX - bX)));
    float distFromRGEdge = ((x * (gY - rY)) - (y * (gX - rX)) + (gX * rY) - (gY * rX)) / distBetweenRG;
    float distFromGBEdge = ((x * (bY - gY)) - (y * (bX - gX)) + (bX * gY) - (bY * gX)) / distBetweenGB;
    float distFromRBEdge = ((x * (rY - bY)) - (y * (rX - bX)) + (rX * bY) - (rY * bX)) / distBetweenRB;

    outDistances[0] = distFromRGEdge;
    outDistances[1] = distFromGBEdge;
    outDistances[2] = distFromRBEdge;
}

static float calcOutofSRGB(clContext * C, float x, float y, clProfilePrimaries * primaries)
{
    COLORIST_UNUSED(C);

    static const clProfilePrimaries srgbPrimaries = { { 0.64f, 0.33f }, { 0.30f, 0.60f }, { 0.15f, 0.06f }, { 0.3127f, 0.3290f } };

    float gamutDistances[3];
    float srgbDistances[3];
    float srgbMaxDist, gamutMaxDist = 0.0f, totalDist, ratio;
    int i;

    if (fabsf(srgbPrimaries.green[1] - primaries->green[1]) < 0.0001f) {
        // We're probably in sRGB, just say we're in-gamut
        return 0;
    }

    calcGamutDistances(x, y, primaries, gamutDistances);
    calcGamutDistances(x, y, &srgbPrimaries, srgbDistances);

    srgbMaxDist = srgbDistances[0];
    for (i = 0; i < 3; ++i) {
        if (srgbMaxDist <= srgbDistances[i]) {
            srgbMaxDist = srgbDistances[i];
            gamutMaxDist = gamutDistances[i];
        }
    }

    if (srgbMaxDist < 0.0002f) {
        // in gamut
        return 0;
    }

    if (gamutMaxDist > -0.00001f) {
        // As far as possible, probably on the line or on a primary
        return 1;
    }

    totalDist = srgbMaxDist - gamutMaxDist;
    ratio = srgbMaxDist / totalDist;

    if (ratio > 0.9999) {
        // close enough
        ratio = 1;
    }
    return ratio;
}

static uint8_t intensityToU8(float intensity)
{
    const float invSRGBGamma = 1.0f / 2.2f;
    intensity = CL_CLAMP(intensity, 0.0f, 1.0f);
    intensity = 255.0f * powf(intensity, invSRGBGamma);
    intensity = CL_CLAMP(intensity, 0.0f, 255.0f);
    return (uint8_t)intensity;
}

clImageSRGBHighlightPixelInfo * clImageSRGBHighlightPixelInfoCreate(struct clContext * C, int pixelCount)
{
    clImageSRGBHighlightPixelInfo * pixelInfo = clAllocateStruct(clImageSRGBHighlightPixelInfo);
    pixelInfo->pixelCount = pixelCount;
    pixelInfo->pixels = clAllocate(sizeof(clImageSRGBHighlightPixel) * pixelCount);
    memset(pixelInfo->pixels, 0, sizeof(clImageSRGBHighlightPixel) * pixelCount);
    return pixelInfo;
}

void clImageSRGBHighlightPixelInfoDestroy(struct clContext * C, clImageSRGBHighlightPixelInfo * pixelInfo)
{
    clFree(pixelInfo->pixels);
    clFree(pixelInfo);
}

clImage * clImageCreateSRGBHighlight(clContext * C,
                                     clImage * srcImage,
                                     int srgbLuminance,
                                     clImageSRGBHighlightStats * stats,
                                     clImageSRGBHighlightPixelInfo * outPixelInfo)
{
    const float minHighlight = 0.4f;

    clTransform * toXYZ = clTransformCreate(C, srcImage->profile, CL_XF_RGBA, NULL, CL_XF_XYZ, CL_TONEMAP_OFF);
    clTransform * fromXYZ = clTransformCreate(C, NULL, CL_XF_XYZ, srcImage->profile, CL_XF_RGB, CL_TONEMAP_OFF);

    clContextLog(C, "highlight", 1, "Creating sRGB highlight (%d nits, %s)...", srgbLuminance, clTransformCMMName(C, toXYZ));

    clProfilePrimaries srcPrimaries;
    clProfileCurve srcCurve;
    int srcLuminance = 0;
    clProfileQuery(C, srcImage->profile, &srcPrimaries, &srcCurve, &srcLuminance);
    if (srcLuminance == CL_LUMINANCE_UNSPECIFIED) {
        if (srcCurve.type == CL_PCT_HLG) {
            srcLuminance = clTransformCalcHLGLuminance(C->defaultLuminance);
        } else {
            srcLuminance = C->defaultLuminance;
        }
    }

    float overbrightScale = (float)srcLuminance * srcCurve.implicitScale / (float)srgbLuminance;

    // clTransformCalcMaxY assumes the RGB profile is linear with a 1 nit luminance
    clProfileCurve gamma1;
    gamma1.type = CL_PCT_GAMMA;
    gamma1.gamma = 1.0f;
    clProfile * linearProfile = clProfileCreate(C, &srcPrimaries, &gamma1, 1, NULL);
    clTransform * linearToXYZ = clTransformCreate(C, linearProfile, CL_XF_RGBA, NULL, CL_XF_XYZ, CL_TONEMAP_OFF);
    clTransform * linearFromXYZ = clTransformCreate(C, NULL, CL_XF_XYZ, linearProfile, CL_XF_RGB, CL_TONEMAP_OFF);

    memset(stats, 0, sizeof(clImageSRGBHighlightStats));
    int pixelCount = stats->pixelCount = srcImage->width * srcImage->height;

    clImagePrepareReadPixels(C, srcImage, CL_PIXELFORMAT_F32);

    float * xyzPixels = clAllocate(3 * sizeof(float) * pixelCount);
    clTransformRun(C, toXYZ, srcImage->pixelsF32, xyzPixels, pixelCount);

    clImageSRGBHighlightPixelInfo * pixelInfo = outPixelInfo;
    if (!pixelInfo) {
        pixelInfo = clImageSRGBHighlightPixelInfoCreate(C, pixelCount);
    }

    clImage * highlight = clImageCreate(C, srcImage->width, srcImage->height, 8, NULL);
    clImagePrepareWritePixels(C, highlight, CL_PIXELFORMAT_U16);
    for (int i = 0; i < pixelCount; ++i) {
        float * srcXYZ = &xyzPixels[i * 3];
        uint16_t * dstPixel = &highlight->pixelsU16[i * CL_CHANNELS_PER_PIXEL];
        clImageSRGBHighlightPixel * pixelHighlightInfo = &pixelInfo->pixels[i];

        cmsCIEXYZ XYZ;
        XYZ.X = srcXYZ[0];
        XYZ.Y = srcXYZ[1];
        XYZ.Z = srcXYZ[2];

        cmsCIExyY xyY;
        if (XYZ.Y > 0) {
            cmsXYZ2xyY(&xyY, &XYZ);
        } else {
            xyY.x = 0.3127f;
            xyY.y = 0.3290f;
            xyY.Y = 0.0f;
        }

        pixelHighlightInfo->x = (float)xyY.x;
        pixelHighlightInfo->y = (float)xyY.y;
        pixelHighlightInfo->Y = (float)xyY.Y / ((float)srcLuminance * srcCurve.implicitScale);

        float pixelNits = (float)xyY.Y;
        if (stats->brightestPixelNits < pixelNits) {
            stats->brightestPixelNits = pixelNits;
            stats->brightestPixelX = i % srcImage->width;
            stats->brightestPixelY = i / srcImage->width;
        }

        float maxY = clTransformCalcMaxY(C, linearFromXYZ, linearToXYZ, (float)xyY.x, (float)xyY.y) * (float)srgbLuminance;
        float overbright = calcOverbright((float)xyY.Y, overbrightScale, maxY);
        float outOfSRGB = calcOutofSRGB(C, (float)xyY.x, (float)xyY.y, &srcPrimaries);

        pixelHighlightInfo->nits = pixelNits;
        pixelHighlightInfo->maxNits = maxY;
        pixelHighlightInfo->outOfGamut = outOfSRGB;

        float baseIntensity = pixelNits / (float)srgbLuminance;
        baseIntensity = CL_CLAMP(baseIntensity, 0.0f, 1.0f);
        uint8_t intensity8 = intensityToU8(baseIntensity);

        if ((overbright > 0.0f) && (outOfSRGB > 0.0f)) {
            float biggerHighlight = (overbright > outOfSRGB) ? overbright : outOfSRGB;
            float highlightIntensity = minHighlight + (biggerHighlight * (1.0f - minHighlight));
            // Yellow
            dstPixel[0] = intensity8;
            dstPixel[1] = intensity8;
            dstPixel[2] = intensityToU8(baseIntensity * (1.0f - highlightIntensity));
            ++stats->bothPixelCount;
        } else if (overbright > 0.0f) {
            float highlightIntensity = minHighlight + (overbright * (1.0f - minHighlight));
            // Magenta
            dstPixel[0] = intensity8;
            dstPixel[1] = intensityToU8(baseIntensity * (1.0f - highlightIntensity));
            dstPixel[2] = intensity8;
            ++stats->overbrightPixelCount;
        } else if (outOfSRGB > 0.0f) {
            float highlightIntensity = minHighlight + (outOfSRGB * (1.0f - minHighlight));
            // Cyan
            dstPixel[0] = intensityToU8(baseIntensity * (1.0f - highlightIntensity));
            dstPixel[1] = intensity8;
            dstPixel[2] = intensity8;
            ++stats->outOfGamutPixelCount;
        } else {
            // Gray
            dstPixel[0] = intensity8;
            dstPixel[1] = intensity8;
            dstPixel[2] = intensity8;
        }
        dstPixel[3] = 255;
    }
    stats->hdrPixelCount = stats->bothPixelCount + stats->overbrightPixelCount + stats->outOfGamutPixelCount;

    if (!outPixelInfo) {
        clImageSRGBHighlightPixelInfoDestroy(C, pixelInfo);
    }

    clTransformDestroy(C, linearToXYZ);
    clTransformDestroy(C, linearFromXYZ);
    clProfileDestroy(C, linearProfile);

    clTransformDestroy(C, fromXYZ);
    clTransformDestroy(C, toXYZ);
    clFree(xyzPixels);
    return highlight;
}
