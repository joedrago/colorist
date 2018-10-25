// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/embedded.h"
#include "colorist/image.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include "cJSON.h"

#include <math.h>
#include <string.h>

#define FAIL() { returnCode = 1; goto reportCleanup; }

// Calculates the max Y for a given xy chromaticity, this is an awful hack
static float calcMaxY(clContext * C, float x, float y, clTransform * fromXYZ, clTransform * toXYZ)
{
    float floatXYZ[3];
    float floatRGB[3];
    float maxChannel;
    cmsCIEXYZ XYZ;
    cmsCIExyY xyY;
    xyY.x = x;
    xyY.y = y;
    xyY.Y = 1.0f; // Filthy lies
    cmsxyY2XYZ(&XYZ, &xyY);
    floatXYZ[0] = (float)XYZ.X;
    floatXYZ[1] = (float)XYZ.Y;
    floatXYZ[2] = (float)XYZ.Z;
    clTransformRun(C, fromXYZ, 1, floatXYZ, floatRGB, 1);
    maxChannel = floatRGB[0];
    if (maxChannel < floatRGB[1])
        maxChannel = floatRGB[1];
    if (maxChannel < floatRGB[2])
        maxChannel = floatRGB[2];
    floatRGB[0] /= maxChannel;
    floatRGB[1] /= maxChannel;
    floatRGB[2] /= maxChannel;
    clTransformRun(C, toXYZ, 1, floatRGB, floatXYZ, 1);
    return floatXYZ[1];
}

static float calcOverbright(clContext * C, float x, float y, float Y, float overbrightScale, clTransform * fromXYZ, clTransform * toXYZ)
{
    float maxY = calcMaxY(C, x, y, fromXYZ, toXYZ);
    float p = (Y / maxY) * overbrightScale;
    if (p > 1.0f) {
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
    static const clProfilePrimaries srgbPrimaries = { { 0.64f, 0.33f }, { 0.30f, 0.60f }, { 0.15f, 0.06f }, { 0.3127f, 0.3290f } };

    float gamutDistances[3];
    float srgbDistances[3];
    float srgbMaxDist, gamutMaxDist, totalDist, ratio;
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

    if (srgbMaxDist < 0) {
        // in gamut
        return 0;
    }

    if (gamutMaxDist > -0.00001) {
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

typedef struct SRGBHighlightStats
{
    int overbrightPixelCount;
    int outOfGamutPixelCount;
    int bothPixelCount; // overbright + out-of-gamut
    int hdrPixelCount;  // the sum of the above values
    int pixelCount;
    int brightestPixelX;
    int brightestPixelY;
    float brightestPixelNits;
} SRGBHighlightStats;

static clImage * createSRGBHighlight(clContext * C, clImage * srcImage, int srgbLuminance, SRGBHighlightStats * stats)
{
    const float minHighlight = 0.4f;
    float luminanceScale;
    float overbrightScale;
    float * srcFloats;
    clProfilePrimaries srcPrimaries;
    clProfileCurve srcCurve;
    int srcLuminance = 0;
    float * xyzPixels;
    int pixelCount = 0;
    clImage * highlight = NULL;
    int i;

    clTransform * toXYZ = clTransformCreate(C, srcImage->profile, CL_XF_RGBA_FLOAT, NULL, CL_XF_XYZ_FLOAT);
    clTransform * fromXYZ = clTransformCreate(C, NULL, CL_XF_XYZ_FLOAT, srcImage->profile, CL_XF_RGB_FLOAT);

    clContextLog(C, "encode", 1, "Creating sRGB highlight (%d nits, %s)...", srgbLuminance, clTransformCMMName(C, toXYZ));

    memset(stats, 0, sizeof(SRGBHighlightStats));
    stats->pixelCount = pixelCount = srcImage->width * srcImage->height;

    clProfileQuery(C, srcImage->profile, &srcPrimaries, &srcCurve, &srcLuminance);
    srcLuminance = (srcLuminance != 0) ? srcLuminance : COLORIST_DEFAULT_LUMINANCE;

    srcFloats = clAllocate(4 * sizeof(float) * pixelCount);
    clPixelMathUNormToFloat(C, srcImage->pixels, srcImage->depth, srcFloats, pixelCount);

    xyzPixels = clAllocate(3 * sizeof(float) * pixelCount);
    clTransformRun(C, toXYZ, C->params.jobs, (uint8_t *)srcFloats, (uint8_t *)xyzPixels, pixelCount);

    highlight = clImageCreate(C, srcImage->width, srcImage->height, 8, NULL);
    luminanceScale = (float)srcLuminance / 300.0f;
    overbrightScale = (float)srcLuminance / (float)srgbLuminance;
    if (srcCurve.matrixCurveScale > 0.0f) {
        // This handles crazy A2B* tags, somewhat
        overbrightScale *= srcCurve.matrixCurveScale;
    }
    for (i = 0; i < pixelCount; ++i) {
        float * srcXYZ = &xyzPixels[i * 3];
        uint8_t * dstPixel = &highlight->pixels[i * 4];
        float baseIntensity;
        uint8_t intensity8;
        float overbright, outOfSRGB;
        float pixelNits;
        cmsCIEXYZ XYZ;
        cmsCIExyY xyY;

        XYZ.X = srcXYZ[0];
        XYZ.Y = srcXYZ[1];
        XYZ.Z = srcXYZ[2];
        if (XYZ.Y > 0) {
            cmsXYZ2xyY(&xyY, &XYZ);
        } else {
            xyY.x = 0.3127f;
            xyY.y = 0.3290f;
            xyY.Y = 0.0f;
        }

        pixelNits = (float)xyY.Y * (float)srcLuminance;
        if (stats->brightestPixelNits < pixelNits) {
            stats->brightestPixelNits = pixelNits;
            stats->brightestPixelX = i % srcImage->width;
            stats->brightestPixelY = i / srcImage->width;
        }

        baseIntensity = luminanceScale * (float)xyY.Y;
        baseIntensity = CL_CLAMP(baseIntensity, 0.0f, 1.0f);
        intensity8 = intensityToU8(baseIntensity);

        overbright = calcOverbright(C, (float)xyY.x, (float)xyY.y, (float)xyY.Y, overbrightScale, fromXYZ, toXYZ);
        outOfSRGB = calcOutofSRGB(C, (float)xyY.x, (float)xyY.y, &srcPrimaries);

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

    clTransformDestroy(C, fromXYZ);
    clTransformDestroy(C, toXYZ);
    clFree(srcFloats);
    clFree(xyzPixels);
    return highlight;
}

static clBool addSRGBHighlight(clContext * C, clImage * image, int maxLuminance, cJSON * payload, const char * name)
{
    clImage * highlight;
    char * pngB64;
    SRGBHighlightStats stats;
    cJSON * base;

    highlight = createSRGBHighlight(C, image, maxLuminance, &stats);
    if (!highlight) {
        return clFalse;
    }
    pngB64 = clContextWriteURI(C, highlight, "png", 0, 0);
    clImageDestroy(C, highlight);
    if (!pngB64) {
        return clFalse;
    }

    base = cJSON_CreateObject();
    cJSON_AddItemToObject(base, "visual", cJSON_CreateString(pngB64));
    cJSON_AddItemToObject(base, "overbrightPixelCount", cJSON_CreateNumber(stats.overbrightPixelCount));
    cJSON_AddItemToObject(base, "outOfGamutPixelCount", cJSON_CreateNumber(stats.outOfGamutPixelCount));
    cJSON_AddItemToObject(base, "bothPixelCount", cJSON_CreateNumber(stats.bothPixelCount));
    cJSON_AddItemToObject(base, "hdrPixelCount", cJSON_CreateNumber(stats.hdrPixelCount));
    cJSON_AddItemToObject(base, "pixelCount", cJSON_CreateNumber(stats.pixelCount));
    cJSON_AddItemToObject(base, "brightestPixelX", cJSON_CreateNumber(stats.brightestPixelX));
    cJSON_AddItemToObject(base, "brightestPixelY", cJSON_CreateNumber(stats.brightestPixelY));
    cJSON_AddItemToObject(base, "brightestPixelNits", cJSON_CreateNumber(stats.brightestPixelNits));
    cJSON_AddItemToObject(payload, name, base);
    clFree(pngB64);
    return clTrue;
}

static clBool reportBasicInfo(clContext * C, clImage * image, cJSON * payload)
{
    Timer t;
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;
    cJSON * jsonICC;
    cJSON * jsonPrimaries;
    char * text;
    clRaw rawProfile;
    char * rawProfileB64;

    jsonICC = cJSON_CreateObject();
    cJSON_AddItemToObject(payload, "icc", jsonICC);

    if (!clProfileQuery(C, image->profile, &primaries, &curve, &maxLuminance)) {
        return clFalse;
    }

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        return clFalse;
    }
    rawProfileB64 = clRawToBase64(C, &rawProfile);
    if (!rawProfileB64) {
        clRawFree(C, &rawProfile);
        return clFalse;
    }
    cJSON_AddItemToObject(jsonICC, "raw", cJSON_CreateString(rawProfileB64));
    clRawFree(C, &rawProfile);
    clFree(rawProfileB64);

    cJSON_AddItemToObject(payload, "width", cJSON_CreateNumber(image->width));
    cJSON_AddItemToObject(payload, "height", cJSON_CreateNumber(image->height));
    cJSON_AddItemToObject(payload, "depth", cJSON_CreateNumber(image->depth));

    {
        char * channelFormat = "u8";
        clRaw rawPixels;
        clStructArraySchema imageSchema[4];
        if (image->depth > 8) {
            channelFormat = "u16";
        }
        imageSchema[0].format = channelFormat;
        imageSchema[0].name = "r";
        imageSchema[1].format = channelFormat;
        imageSchema[1].name = "g";
        imageSchema[2].format = channelFormat;
        imageSchema[2].name = "b";
        imageSchema[3].format = channelFormat;
        imageSchema[3].name = "a";
        rawPixels.ptr = image->pixels;
        rawPixels.size = image->size;
        clContextLog(C, "encode", 0, "Packing raw pixels...");
        timerStart(&t);
        cJSON_AddItemToObject(payload, "raw", clRawToStructArray(C, &rawPixels, image->width, image->height, imageSchema, 4));
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    {
        clConversionParams params;
        clImage * visual;
        char * pngB64;
        clContextLog(C, "encode", 0, "Creating raw pixels visual...");
        timerStart(&t);
        clConversionParamsSetDefaults(C, &params);
        params.formatName = "jpg";
        params.bpp = 8;
        clContextGetRawStockPrimaries(C, "bt709", params.primaries);
        params.gamma = COLORIST_SRGB_GAMMA;
        params.luminance = COLORIST_DEFAULT_LUMINANCE;
        params.quality = 95;
        params.jobs = C->params.jobs;
        visual = clImageConvert(C, image, &params, NULL);
        if (!visual) {
            return clFalse;
        }
        clContextLog(C, "encode", 1, "Generating Base64 encoded PNG...");
        pngB64 = clContextWriteURI(C, visual, "png", 0, 0);
        clImageDestroy(C, visual);
        if (!pngB64) {
            return clFalse;
        }
        cJSON_AddItemToObject(payload, "visual", cJSON_CreateString(pngB64));
        clFree(pngB64);
        clContextLog(C, "encode", 0, "Visual generation complete.");
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    {
        clContextLog(C, "encode", 0, "Creating out-of-gamut highlights...");
        timerStart(&t);

        // if (!addSRGBHighlight(C, image, 100, payload, "srgb100")) {
        //     return clFalse;
        // }
        if (!addSRGBHighlight(C, image, 300, payload, "srgb300")) {
            return clFalse;
        }

        clContextLog(C, "encode", 0, "Highlight generation complete.");
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    text = clProfileGetMLU(C, image->profile, "desc", "en", "US");
    if (text == NULL) {
        text = clContextStrdup(C, "Unknown");
    }
    cJSON_AddItemToObject(jsonICC, "description", cJSON_CreateString(text));
    clFree(text);

    if (clProfileHasPQSignature(C, image->profile, &primaries)) {
        curve.gamma = 0.0f;
        maxLuminance = 10000;
        cJSON_AddItemToObject(jsonICC, "pq", cJSON_CreateBool(clTrue));
    } else {
        // Check for profiles that we can't make valid reports for
        if (curve.type != CL_PCT_GAMMA) {
            clContextLogError(C, "Can't create report: the supplied tone curve can't be interpreted by current report JS");
            return clFalse;
        }
    }

    jsonPrimaries = cJSON_CreateArray();
    {
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.red[0]));
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.red[1]));
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.green[0]));
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.green[1]));
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.blue[0]));
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.blue[1]));
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.white[0]));
        cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.white[1]));
    }

    cJSON_AddItemToObject(jsonICC, "primaries", jsonPrimaries);
    cJSON_AddItemToObject(jsonICC, "gamma", cJSON_CreateNumber(curve.gamma));
    cJSON_AddItemToObject(jsonICC, "luminance", cJSON_CreateNumber(maxLuminance));
    return clTrue;
}

int clContextReport(clContext * C)
{
    Timer overall, t;
    int returnCode = 0;

    clImage * image = NULL;

    cJSON * payload = cJSON_CreateObject();
    FILE * outf = NULL;

    clContextLog(C, "action", 0, "Report: %s -> %s", C->inputFilename, C->outputFilename);
    timerStart(&overall);

    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    timerStart(&t);
    image = clContextRead(C, C->inputFilename, C->iccOverrideIn, NULL);
    if (image == NULL) {
        return 1;
    }
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    // Basic Info
    {
        timerStart(&t);
        cJSON_AddItemToObject(payload, "filename", cJSON_CreateString(C->inputFilename));
        if (!reportBasicInfo(C, image, payload)) {
            FAIL();
        }
    }

    timerStart(&t);
    {
        static const char payloadPrefix[] = "var COLORIST_DATA = ";
        static const char coloristDataMarker[] = "__COLORIST_DATA__";
        const char * coloristDataInjectLoc = strstr((const char *)reportTemplateBinaryData, coloristDataMarker);
        const char * afterPtr;
        int beforeLen, afterLen;
        char * payloadString;
        if (!coloristDataInjectLoc) {
            clContextLogError(C, "Template does not contain the string \"%s\", bailing out", coloristDataMarker);
            FAIL();
        }

        beforeLen = (int)(coloristDataInjectLoc - (const char *)reportTemplateBinaryData);
        afterPtr = coloristDataInjectLoc + strlen(coloristDataMarker);
        afterLen = strlen(afterPtr);

        outf = fopen(C->outputFilename, "wb");
        if (!outf) {
            clContextLogError(C, "Cant open report file for write: %s", C->outputFilename);
            FAIL();
        }

        payloadString = cJSON_Print(payload);
        if (!payloadString) {
            clContextLogError(C, "failed to create payload string!");
            FAIL();
        }

        fwrite(reportTemplateBinaryData, beforeLen, 1, outf);
        fwrite(payloadPrefix, strlen(payloadPrefix), 1, outf);
        fwrite(payloadString, strlen(payloadString), 1, outf);
        fwrite(afterPtr, afterLen, 1, outf);
        fclose(outf);
        outf = NULL;
    }

    clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

reportCleanup:
    if (image)
        clImageDestroy(C, image);

    cJSON_Delete(payload);
    if (outf)
        fclose(outf);

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "Conversion complete.");
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
}
