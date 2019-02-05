// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2019.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include "apg.h"

#include <string.h>

struct clImage * clFormatReadAPG(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteAPG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

static void dumpAPG(struct clContext * C, apgImage * apg, uint32_t totalSize);

struct clImage * clFormatReadAPG(struct clContext * C, const char * formatName, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(input);
    COLORIST_UNUSED(C);

    clImage * image = NULL;
    clProfile * profile = NULL;

    apgResult result;
    apgImage * apg = apgImageDecode(input->ptr, (uint32_t)input->size, &result);
    if (!apg) {
        clContextLogError(C, "Failed get ICC profile chunk");
        goto readCleanup;
    }

    if (apg->icc && apg->iccSize) {
        profile = clProfileParse(C, apg->icc, apg->iccSize, NULL);
        if (!profile) {
            clContextLogError(C, "Failed parse ICC profile chunk");
            goto readCleanup;
        }
    }

    image = clImageCreate(C, apg->width, apg->height, apg->depth, profile);

    int pixelChannelCount = CL_CHANNELS_PER_PIXEL * image->width * image->height;
    for (int i = 0; i < pixelChannelCount; ++i) {
        image->pixels[i] = apg->pixels[i];
    }

    if (C->verbose) {
        dumpAPG(C, apg, (uint32_t)input->size);
    }

readCleanup:
    if (apg) {
        apgImageDestroy(apg);
    }
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;
}

clBool clFormatWriteAPG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);

    clBool writeResult = clTrue;
    apgImage * apg = NULL;

    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        writeResult = clFalse;
        goto writeCleanup;
    }

    apg = apgImageCreate(image->width, image->height, image->depth);
    apgImageSetICC(apg, rawProfile.ptr, (uint32_t)rawProfile.size);

    // Calculate proper YUV coefficients
    clProfilePrimaries primaries;
    clProfileQuery(C, image->profile, &primaries, NULL, NULL);
    clProfileCurve gamma1;
    gamma1.type = CL_PCT_GAMMA;
    gamma1.gamma = 1.0f;
    clProfile * linearProfile = clProfileCreate(C, &primaries, &gamma1, 1, NULL);
    clTransform * linearToXYZ = clTransformCreate(C, linearProfile, CL_XF_RGBA, 32, NULL, CL_XF_XYZ, 32, CL_TONEMAP_OFF);
    clTransform * linearFromXYZ = clTransformCreate(C, NULL, CL_XF_XYZ, 32, linearProfile, CL_XF_RGB, 32, CL_TONEMAP_OFF);
    float kr = clTransformCalcMaxY(C, linearFromXYZ, linearToXYZ, primaries.red[0], primaries.red[1]);
    float kg = clTransformCalcMaxY(C, linearFromXYZ, linearToXYZ, primaries.green[0], primaries.green[1]);
    float kb = clTransformCalcMaxY(C, linearFromXYZ, linearToXYZ, primaries.blue[0], primaries.blue[1]);
    apgImageSetYUVCoefficients(apg, kr, kg, kb);
    clTransformDestroy(C, linearToXYZ);
    clTransformDestroy(C, linearFromXYZ);
    clProfileDestroy(C, linearProfile);

    int pixelChannelCount = CL_CHANNELS_PER_PIXEL * image->width * image->height;
    for (int i = 0; i < pixelChannelCount; ++i) {
        apg->pixels[i] = image->pixels[i];
    }

    apgResult result = apgImageEncode(apg, writeParams->quality);
    if (result != APG_RESULT_OK) {
        clContextLogError(C, "APG encoder failed: Error Code: %d", (int)result);
        writeResult = clFalse;
        goto writeCleanup;
    }

    if (!apg->encoded || !apg->encodedSize) {
        clContextLogError(C, "APG encoder returned empty data");
        writeResult = clFalse;
        goto writeCleanup;
    }

    clRawSet(C, output, apg->encoded, apg->encodedSize);

    if (C->verbose) {
        dumpAPG(C, apg, apg->encodedSize);
    }

writeCleanup:
    if (apg) {
        apgImageDestroy(apg);
    }
    clRawFree(C, &rawProfile);
    return writeResult;
}

static void dumpAPG(struct clContext * C, apgImage * apg, uint32_t totalSize)
{
    COLORIST_UNUSED(apg);
    clContextLog(C, "apg", 0, "APG YUV coeff. (Red)  : %.3f (%u)", (float)apg->yuvKR / 65535.0f, apg->yuvKR);
    clContextLog(C, "apg", 0, "APG YUV coeff. (Green): %.3f (%u)", (float)apg->yuvKG / 65535.0f, apg->yuvKG);
    clContextLog(C, "apg", 0, "APG YUV coeff. (Blue) : %.3f (%u)", (float)apg->yuvKB / 65535.0f, apg->yuvKB);
    clContextLog(C, "apg", 0, "APG ICC size          : %u", apg->iccSize);
    clContextLog(C, "apg", 0, "APG color OBU size    : %u", apg->extraInfo.colorPayloadSize);
    clContextLog(C, "apg", 0, "APG alpha OBU size    : %u", apg->extraInfo.alphaPayloadSize);
    clContextLog(C, "apg", 0, "APG total size        : %u", totalSize);
}
