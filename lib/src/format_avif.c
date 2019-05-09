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

#include "avif/avif.h"

#include <string.h>

static clProfile * nclxToclProfile(struct clContext * C, avifNclxColorProfile * nclx);
static clBool clProfileToNclx(struct clContext * C, struct clProfile * profile, avifNclxColorProfile * nclx);
static void logAvifImage(struct clContext * C, avifImage * avif);

struct clImage * clFormatReadAVIF(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteAVIF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadAVIF(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(input);
    COLORIST_UNUSED(C);

    clImage * image = NULL;
    clProfile * profile = NULL;

    avifRawData raw;
    raw.data = input->ptr;
    raw.size = input->size;

    avifImage * avif = avifImageCreateEmpty();
    avifResult decodeResult = avifImageRead(avif, &raw);
    if ((decodeResult != AVIF_RESULT_OK) || !avif->width || !avif->height) {
        clContextLogError(C, "Failed to decode AVIF (%s)", avifResultToString(decodeResult));
        goto readCleanup;
    }

    if (overrideProfile) {
        profile = clProfileClone(C, overrideProfile);
    } else if (avif->profileFormat == AVIF_PROFILE_FORMAT_NCLX) {
        profile = nclxToclProfile(C, &avif->nclx);
    } else if (avif->profileFormat == AVIF_PROFILE_FORMAT_ICC) {
        profile = clProfileParse(C, avif->icc.data, avif->icc.size, NULL);
        if (!profile) {
            clContextLogError(C, "Failed parse ICC profile chunk");
            goto readCleanup;
        }
    }

    logAvifImage(C, avif);

    image = clImageCreate(C, avif->width, avif->height, avif->depth, profile);

    avifBool usesU16 = avifImageUsesU16(avif);
    int maxChannel = (1 << image->depth) - 1;
    if (usesU16) {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * pixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                pixel[0] = *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_R][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_R])]);
                pixel[1] = *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_G][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_G])]);
                pixel[2] = *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_B][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_B])]);
                if (avif->alphaPlane) {
                    pixel[3] = *((uint16_t *)&avif->alphaPlane[(i * 2) + (j * avif->alphaRowBytes)]);
                } else {
                    pixel[3] = (uint16_t)maxChannel;
                }
            }
        }
    } else {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * pixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                pixel[0] = avif->rgbPlanes[AVIF_CHAN_R][i + (j * avif->rgbRowBytes[AVIF_CHAN_R])];
                pixel[1] = avif->rgbPlanes[AVIF_CHAN_G][i + (j * avif->rgbRowBytes[AVIF_CHAN_G])];
                pixel[2] = avif->rgbPlanes[AVIF_CHAN_B][i + (j * avif->rgbRowBytes[AVIF_CHAN_B])];
                if (avif->alphaPlane) {
                    pixel[3] = avif->alphaPlane[i + (j * avif->alphaRowBytes)];
                } else {
                    pixel[3] = (uint8_t)maxChannel;
                }
            }
        }
    }

readCleanup:
    avifImageDestroy(avif);
    if (profile) {
        clProfileDestroy(C, profile);
    }

    return image;
}

clBool clFormatWriteAVIF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);

    clBool writeResult = clTrue;
    avifImage * avif = NULL;

    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        writeResult = clFalse;
        goto writeCleanup;
    }

    avifPixelFormat avifYUVFormat;
    switch (writeParams->yuvFormat) {
        case CL_YUVFORMAT_444:  avifYUVFormat = AVIF_PIXEL_FORMAT_YUV444; break;
        case CL_YUVFORMAT_422:  avifYUVFormat = AVIF_PIXEL_FORMAT_YUV422; break;
        case CL_YUVFORMAT_420:  avifYUVFormat = AVIF_PIXEL_FORMAT_YUV420; break;
        case CL_YUVFORMAT_YV12: avifYUVFormat = AVIF_PIXEL_FORMAT_YV12;   break;
        case CL_YUVFORMAT_AUTO:
        case CL_YUVFORMAT_INVALID:
        default:
            clContextLogError(C, "Unable to choose AVIF YUV format");
            writeResult = clFalse;
            goto writeCleanup;
    }

    avif = avifImageCreate(image->width, image->height, image->depth, avifYUVFormat);

    if (writeParams->writeProfile) {
        avifNclxColorProfile nclx;
        if (clProfileToNclx(C, image->profile, &nclx)) {
            clContextLog(C, "avif", 1, "Writing colr box (nclx): C: %d / T: %d / M: %d / F: 0x%x",
                nclx.colourPrimaries, nclx.transferCharacteristics, nclx.matrixCoefficients, nclx.fullRangeFlag);
            avifImageSetProfileNCLX(avif, &nclx);
        } else {
            clContextLog(C, "avif", 1, "Writing colr box (icc): %u bytes", (uint32_t)rawProfile.size);
            avifImageSetProfileICC(avif, rawProfile.ptr, rawProfile.size);
        }
    }

    avifImageAllocatePlanes(avif, AVIF_PLANES_RGB | AVIF_PLANES_A);
    avifRawData avifOutput = AVIF_RAW_DATA_EMPTY;

    avifBool usesU16 = avifImageUsesU16(avif);
    for (int j = 0; j < image->height; ++j) {
        for (int i = 0; i < image->width; ++i) {
            uint16_t * pixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
            if (usesU16) {
                *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_R][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_R])]) = pixel[0];
                *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_G][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_G])]) = pixel[1];
                *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_B][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_B])]) = pixel[2];
                *((uint16_t *)&avif->alphaPlane[(i * 2) + (j * avif->alphaRowBytes)]) = pixel[3];
            } else {
                avif->rgbPlanes[AVIF_CHAN_R][i + (j * avif->rgbRowBytes[AVIF_CHAN_R])] = (uint8_t)pixel[0];
                avif->rgbPlanes[AVIF_CHAN_G][i + (j * avif->rgbRowBytes[AVIF_CHAN_G])] = (uint8_t)pixel[1];
                avif->rgbPlanes[AVIF_CHAN_B][i + (j * avif->rgbRowBytes[AVIF_CHAN_B])] = (uint8_t)pixel[2];
                avif->alphaPlane[i + (j * avif->alphaRowBytes)] = (uint8_t)pixel[3];
            }
        }
    }

    int rescaledQuality = 63 - (int)(((float)writeParams->quality / 100.0f) * 63.0f);

    avifResult encodeResult = avifImageWrite(avif, &avifOutput, C->params.jobs, rescaledQuality);
    if (encodeResult != AVIF_RESULT_OK) {
        clContextLogError(C, "AVIF encoder failed (%s)", avifResultToString(encodeResult));
        writeResult = clFalse;
        goto writeCleanup;
    }

    if (!avifOutput.data || !avifOutput.size) {
        clContextLogError(C, "AVIF encoder returned empty data");
        writeResult = clFalse;
        goto writeCleanup;
    }

    clRawSet(C, output, avifOutput.data, avifOutput.size);

    logAvifImage(C, avif);

writeCleanup:
    if (avif) {
        avifImageDestroy(avif);
    }
    avifRawDataFree(&avifOutput);
    clRawFree(C, &rawProfile);
    return writeResult;
}

static clProfile * nclxToclProfile(struct clContext * C, avifNclxColorProfile * nclx)
{
    COLORIST_UNUSED(nclx);

    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;

    // Defaults
    clContextGetStockPrimaries(C, "bt709", &primaries);
    curve.type = CL_PCT_GAMMA;
    curve.gamma = 2.2f;
    curve.implicitScale = 1.0f;
    maxLuminance = CL_LUMINANCE_UNSPECIFIED;

    float prim[8];
    avifNclxColourPrimariesGetValues(nclx->colourPrimaries, prim);
    primaries.red[0] = prim[0];
    primaries.red[1] = prim[1];
    primaries.green[0] = prim[2];
    primaries.green[1] = prim[3];
    primaries.blue[0] = prim[4];
    primaries.blue[1] = prim[5];
    primaries.white[0] = prim[6];
    primaries.white[1] = prim[7];

    switch (nclx->transferCharacteristics) {
        case AVIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_HLG:
            curve.type = CL_PCT_HLG;
            curve.gamma = 1.0f;
            break;
        case AVIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_PQ:
            curve.type = CL_PCT_PQ;
            curve.gamma = 1.0f;
            maxLuminance = 10000;
            break;
        case AVIF_NCLX_TRANSFER_CHARACTERISTICS_GAMMA22:
            curve.type = CL_PCT_GAMMA;
            curve.gamma = 2.2f;
            break;
        case AVIF_NCLX_TRANSFER_CHARACTERISTICS_GAMMA28:
            curve.type = CL_PCT_GAMMA;
            curve.gamma = 2.8f;
            break;
        default:
            clContextLog(C, "avif", 1, "WARNING: Unsupported colr (nclx) transfer_characteristics %d, using gamma:%1.1f, lum:%d", nclx->colourPrimaries, curve.gamma, maxLuminance);
            break;
    }

    char gammaString[64];
    if (curve.type == CL_PCT_PQ) {
        gammaString[0] = 0;
    } else {
        sprintf(gammaString, "(%.2g)", curve.gamma);
    }

    char maxLumString[64];
    if (maxLuminance == CL_LUMINANCE_UNSPECIFIED) {
        strcpy(maxLumString, "Unspecified");
    } else {
        sprintf(maxLumString, "%d", maxLuminance);
    }

    clContextLog(C, "avif", 1, "nclx to ICC: Primaries: (r:%.4g,%.4g g:%.4g,%.4g b:%.4g,%.4g w:%.4g,%.4g), Curve: %s%s, maxLum: %s",
        primaries.red[0], primaries.red[1], primaries.green[0], primaries.green[1], primaries.blue[0], primaries.blue[1], primaries.white[0], primaries.white[1],
        clProfileCurveTypeToString(C, curve.type), gammaString, maxLumString);

    char * description = clGenerateDescription(C, &primaries, &curve, maxLuminance);
    clProfile * profile = clProfileCreate(C, &primaries, &curve, maxLuminance, description);
    clFree(description);
    return profile;
}

static clBool clProfileToNclx(struct clContext * C, struct clProfile * profile, avifNclxColorProfile * nclx)
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance = 0;
    if (!clProfileQuery(C, profile, &primaries, &curve, &luminance)) {
        return clFalse;
    }

    const char * primariesName = NULL;
    float floatPrimaries[8];
    floatPrimaries[0] = primaries.red[0];
    floatPrimaries[1] = primaries.red[1];
    floatPrimaries[2] = primaries.green[0];
    floatPrimaries[3] = primaries.green[1];
    floatPrimaries[4] = primaries.blue[0];
    floatPrimaries[5] = primaries.blue[1];
    floatPrimaries[6] = primaries.white[0];
    floatPrimaries[7] = primaries.white[1];
    avifNclxColourPrimaries foundNclxPrimaries = avifNclxColourPrimariesFind(floatPrimaries, &primariesName);
    if (foundNclxPrimaries == AVIF_NCLX_COLOUR_PRIMARIES_UNKNOWN) {
        return clFalse;
    }

    avifNclxMatrixCoefficients matrixCoefficients = AVIF_NCLX_MATRIX_COEFFICIENTS_UNSPECIFIED;
    switch (foundNclxPrimaries) {
        case AVIF_NCLX_COLOUR_PRIMARIES_BT709:
            matrixCoefficients = AVIF_NCLX_MATRIX_COEFFICIENTS_BT709;
            break;
        case AVIF_NCLX_COLOUR_PRIMARIES_BT2020:
            if ((luminance == CL_LUMINANCE_UNSPECIFIED) || (curve.type == CL_PCT_HLG))
                matrixCoefficients = AVIF_NCLX_MATRIX_COEFFICIENTS_BT2020_NCL;
            else
                matrixCoefficients = AVIF_NCLX_MATRIX_COEFFICIENTS_BT2020_CL;
            break;
        default:
            if ((luminance == CL_LUMINANCE_UNSPECIFIED) || (curve.type == CL_PCT_HLG))
                matrixCoefficients = AVIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL;
            else
                matrixCoefficients = AVIF_NCLX_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL;
            break;
    }

    const char * transferCharacteristicsName = NULL;
    avifNclxTransferCharacteristics transferCharacteristics = AVIF_NCLX_TRANSFER_CHARACTERISTICS_UNKNOWN;
    if ((curve.type == CL_PCT_PQ) && (luminance == 10000)) {
        transferCharacteristics = AVIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_PQ;
        transferCharacteristicsName = "PQ";
    } else {
        if (luminance != CL_LUMINANCE_UNSPECIFIED) {
            // Other than PQ, there is no current way to specify a max luminance via nclx. Bail out!
            return clFalse;
        }

        if (curve.type == CL_PCT_HLG) {
            transferCharacteristics = AVIF_NCLX_TRANSFER_CHARACTERISTICS_BT2100_HLG;
            transferCharacteristicsName = "HLG";
        } else if (curve.type == CL_PCT_GAMMA) {
            if (fabsf(curve.gamma - 2.2f) < 0.001f) {
                transferCharacteristics = AVIF_NCLX_TRANSFER_CHARACTERISTICS_GAMMA22;
                transferCharacteristicsName = "2.2g";
            } else if (fabsf(curve.gamma - 2.8f) < 0.001f) {
                transferCharacteristics = AVIF_NCLX_TRANSFER_CHARACTERISTICS_GAMMA28;
                transferCharacteristicsName = "2.8g";
            }
        }
    }

    if (transferCharacteristics == AVIF_NCLX_TRANSFER_CHARACTERISTICS_UNKNOWN) {
        return clFalse;
    }

    clContextLog(C, "avif", 1, "%s %s color profile detected; switching to nclx colr box.", primariesName, transferCharacteristicsName);
    nclx->colourPrimaries = foundNclxPrimaries;
    nclx->transferCharacteristics = transferCharacteristics;
    nclx->matrixCoefficients = matrixCoefficients;
    nclx->fullRangeFlag = AVIF_NCLX_FULL_RANGE;
    return clTrue;
}

static void logAvifImage(struct clContext * C, avifImage * avif)
{
    const char * yuvFormatString = "Unknown";
    clYUVFormat yuvFormat = CL_YUVFORMAT_INVALID;
    switch (avif->yuvFormat) {
        case AVIF_PIXEL_FORMAT_YUV444:  yuvFormat = CL_YUVFORMAT_444;  break;
        case AVIF_PIXEL_FORMAT_YUV422:  yuvFormat = CL_YUVFORMAT_422;  break;
        case AVIF_PIXEL_FORMAT_YUV420:  yuvFormat = CL_YUVFORMAT_420;  break;
        case AVIF_PIXEL_FORMAT_YV12:    yuvFormat = CL_YUVFORMAT_YV12; break;
        case AVIF_PIXEL_FORMAT_NONE:
        default:
            break;
    }
    if (yuvFormat != CL_YUVFORMAT_INVALID) {
        yuvFormatString = clYUVFormatToString(C, yuvFormat);
    }

    clContextLog(C, "avif", 1, "YUV: %s / ColorOBU: %zub / AlphaOBU: %zub", yuvFormatString, avif->ioStats.colorOBUSize, avif->ioStats.alphaOBUSize);
}
