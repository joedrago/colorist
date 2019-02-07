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

struct clImage * clFormatReadAVIF(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteAVIF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadAVIF(struct clContext * C, const char * formatName, struct clRaw * input)
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
        clContextLogError(C, "Failed to decode AVIF (%d)", (int)decodeResult);
        goto readCleanup;
    }

#if 0
    if (... icc ...) {
        profile = clProfileParse(C, avif->icc, avif->iccSize, NULL);
        if (!profile) {
            clContextLogError(C, "Failed parse ICC profile chunk");
            goto readCleanup;
        }
    }
#endif

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

#if 0
    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        writeResult = clFalse;
        goto writeCleanup;
    }
#endif

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

    avifResult encodeResult = avifImageWrite(avif, &avifOutput, rescaledQuality);
    if (encodeResult != AVIF_RESULT_OK) {
        clContextLogError(C, "AVIF encoder failed: Error Code: %d", (int)encodeResult);
        writeResult = clFalse;
        goto writeCleanup;
    }

    if (!avifOutput.data || !avifOutput.size) {
        clContextLogError(C, "AVIF encoder returned empty data");
        writeResult = clFalse;
        goto writeCleanup;
    }

    clRawSet(C, output, avifOutput.data, avifOutput.size);

writeCleanup:
    if (avif) {
        avifImageDestroy(avif);
    }
    avifRawDataFree(&avifOutput);
    // clRawFree(C, &rawProfile);

    return writeResult;
}
