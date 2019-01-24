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

    avifImage * avif = avifImageCreate();
    avifImage * rgba = avifImageCreate();
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

    avifResult reformatResult = avifImageReformatPixels(avif, rgba, AVIF_PIXEL_FORMAT_RGBA, avif->depth);
    if (reformatResult != AVIF_RESULT_OK) {
        clContextLogError(C, "Failed to reformat AVIF to RGBA (%d)", (int)reformatResult);
        goto readCleanup;
    }

    image = clImageCreate(C, avif->width, avif->height, avif->depth, profile);

    if (image->depth == 8) {
        uint8_t * pixels = image->pixels;
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * pixel = &pixels[4 * (i + (j * image->width))];
                for (int plane = 0; plane < 4; ++plane) {
                    pixel[plane] = (uint8_t)rgba->planes[plane][i + (j * rgba->strides[plane])];
                }
            }
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * pixel = &pixels[4 * (i + (j * image->width))];
                for (int plane = 0; plane < 4; ++plane) {
                    pixel[plane] = rgba->planes[plane][i + (j * rgba->strides[plane])];
                }
            }
        }
    }

readCleanup:
    avifImageDestroy(avif);
    avifImageDestroy(rgba);
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;
}

clBool clFormatWriteAVIF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);

    clBool writeResult = clTrue;

#if 0
    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        writeResult = clFalse;
        goto writeCleanup;
    }
#endif

    avifImage * avif = avifImageCreate();
    avifImageCreatePixels(avif, AVIF_PIXEL_FORMAT_RGBA, image->width, image->height, image->depth);
    avifRawData avifOutput = AVIF_RAW_DATA_EMPTY;

    if (image->depth == 8) {
        uint8_t * pixels = image->pixels;
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * pixel = &pixels[4 * (i + (j * image->width))];
                for (int plane = 0; plane < 4; ++plane) {
                    avif->planes[plane][i + (j * avif->strides[plane])] = pixel[plane];
                }
            }
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * pixel = &pixels[4 * (i + (j * image->width))];
                for (int plane = 0; plane < 4; ++plane) {
                    avif->planes[plane][i + (j * avif->strides[plane])] = pixel[plane];
                }
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
