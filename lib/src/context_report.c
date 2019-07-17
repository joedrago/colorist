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

#define FAIL()              \
    {                       \
        returnCode = 1;     \
        goto reportCleanup; \
    }

static clBool addSRGBHighlight(clContext * C, clImage * image, int maxLuminance, cJSON * payload, const char * name)
{
    clImage * highlight;
    char * pngB64;
    clImageSRGBHighlightStats stats;
    cJSON * base;
    cJSON * highlightInfo = NULL;

    highlight = clImageCreateSRGBHighlight(C, image, maxLuminance, &stats, NULL, &highlightInfo);
    if (!highlight) {
        return clFalse;
    }
    clWriteParams writeParams;
    clWriteParamsSetDefaults(C, &writeParams);
    pngB64 = clContextWriteURI(C, highlight, "png", &writeParams);
    clImageDestroy(C, highlight);
    if (!pngB64) {
        return clFalse;
    }

    base = cJSON_CreateObject();
    cJSON_AddItemToObject(base, "visual", cJSON_CreateString(pngB64));
    cJSON_AddItemToObject(base, "info", highlightInfo);
    cJSON_AddItemToObject(base, "highlightLuminance", cJSON_CreateNumber(maxLuminance));
    cJSON_AddItemToObject(base, "hlgLuminance", cJSON_CreateNumber(clTransformCalcHLGLuminance(maxLuminance)));
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
    char * rawProfileB64;

    jsonICC = cJSON_CreateObject();
    cJSON_AddItemToObject(payload, "icc", jsonICC);

    if (!clProfileQuery(C, image->profile, &primaries, &curve, &maxLuminance)) {
        return clFalse;
    }

    clRaw rawProfile = CL_RAW_EMPTY;
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
        char * channelFormat = "u16";
        clRaw rawPixels;
        clStructArraySchema imageSchema[4];
        imageSchema[0].format = channelFormat;
        imageSchema[0].name = "r";
        imageSchema[1].format = channelFormat;
        imageSchema[1].name = "g";
        imageSchema[2].format = channelFormat;
        imageSchema[2].name = "b";
        imageSchema[3].format = channelFormat;
        imageSchema[3].name = "a";
        rawPixels.ptr = (uint8_t *)image->pixels;
        rawPixels.size = image->size;
        clContextLog(C, "encode", 0, "Packing raw pixels...");
        timerStart(&t);
        cJSON_AddItemToObject(payload, "raw", clRawToStructArray(C, &rawPixels, image->width, image->height, imageSchema, 4));
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    {
        clImage * visual;
        char * pngB64;
        clContextLog(C, "encode", 0, "Creating raw pixels visual...");
        timerStart(&t);
        visual = clImageConvert(C, image, C->params.jobs, 8, NULL, CL_TONEMAP_AUTO);
        if (!visual) {
            return clFalse;
        }
        clContextLog(C, "encode", 1, "Generating Base64 encoded PNG...");
        clWriteParams writeParams;
        clWriteParamsSetDefaults(C, &writeParams);
        pngB64 = clContextWriteURI(C, visual, "png", &writeParams);
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
        clContextLog(C, "highlight", 0, "Creating out-of-gamut highlights...");
        timerStart(&t);

        if (!addSRGBHighlight(C, image, C->defaultLuminance, payload, "srgb")) {
            return clFalse;
        }

        clContextLog(C, "highlight", 0, "Highlight generation complete.");
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
        cJSON_AddItemToObject(jsonICC, "curveType", cJSON_CreateString("pq"));
    } else if ((curve.type == CL_PCT_HLG) || (curve.type == CL_PCT_PQ)) {
        curve.gamma = 0.0f;
        cJSON_AddItemToObject(jsonICC, "curveType", cJSON_CreateString(clProfileCurveTypeToLowercaseString(C, curve.type)));
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

    cJSON * payload = cJSON_CreateObject();
    FILE * outf = NULL;

    clContextLog(C, "action", 0, "Report: %s -> %s", C->inputFilename, C->outputFilename);
    timerStart(&overall);

    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    timerStart(&t);
    clImage * image = clContextRead(C, C->inputFilename, C->iccOverrideIn, NULL);
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
        size_t beforeLen, afterLen;
        char * payloadString;
        if (!coloristDataInjectLoc) {
            clContextLogError(C, "Template does not contain the string \"%s\", bailing out", coloristDataMarker);
            FAIL();
        }

        beforeLen = (coloristDataInjectLoc - (const char *)reportTemplateBinaryData);
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
