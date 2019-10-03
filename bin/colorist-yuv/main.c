// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2019.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include "avif/avif.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char * argv[])
{
    if (argc < 4) {
        printf("Syntax: colorist-yuv [inputFilename] [outputFilename] [444|422|420]\n");
        return 0;
    }

    const char * inputFilename = argv[1];
    const char * outputFilename = argv[2];
    const char * yuvFormatString = argv[3];
    avifPixelFormat avifYUVFormat = AVIF_PIXEL_FORMAT_YUV444;
    if (!strcmp(yuvFormatString, "422")) {
        avifYUVFormat = AVIF_PIXEL_FORMAT_YUV422;
    } else if (!strcmp(yuvFormatString, "420")) {
        avifYUVFormat = AVIF_PIXEL_FORMAT_YUV420;
    }

    clContext * C = clContextCreate(NULL);

    clImage * image = clContextRead(C, inputFilename, NULL, NULL);
    if (image && (image->depth > 8)) {
        avifImage * avif = avifImageCreate(image->width, image->height, image->depth, avifYUVFormat);
        avifImageAllocatePlanes(avif, AVIF_PLANES_RGB | AVIF_PLANES_A);

        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * pixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_R][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_R])]) = pixel[0];
                *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_G][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_G])]) = pixel[1];
                *((uint16_t *)&avif->rgbPlanes[AVIF_CHAN_B][(i * 2) + (j * avif->rgbRowBytes[AVIF_CHAN_B])]) = pixel[2];
                *((uint16_t *)&avif->alphaPlane[(i * 2) + (j * avif->alphaRowBytes)]) = pixel[3];
            }
        }

        avifImageRGBToYUV(avif);
        avifImageYUVToRGB(avif);

        int maxChannel = (1 << image->depth) - 1;
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

        clWriteParams writeParams;
        clWriteParamsSetDefaults(C, &writeParams);
        if (clContextWrite(C, image, outputFilename, NULL, &writeParams)) {
            printf("Wrote: %s\n", outputFilename);
        }
        clImageDestroy(C, image);
    }

    clContextDestroy(C);
    return 0;
}
