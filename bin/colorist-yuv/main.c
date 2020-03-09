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

        clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U16);

        avifRGBImage rgb;
        rgb.width = image->width;
        rgb.height = image->height;
        rgb.depth = image->depth;
        rgb.format = AVIF_RGB_FORMAT_RGBA;
        rgb.pixels = (uint8_t *)image->pixelsU16;
        rgb.rowBytes = image->width * sizeof(uint16_t) * CL_CHANNELS_PER_PIXEL;

        avifImageRGBToYUV(avif, &rgb);
        memset(rgb.pixels, 0, rgb.height * rgb.rowBytes);
        avifImageYUVToRGB(avif, &rgb);

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
