// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/profile.h"

#include <stdio.h>

#define COLORIST_DUMP_DIM 4

static void dumpPixel(clImage * image, int x, int y);

void clImageDebugDump(clImage * image, int x, int y, int w, int h)
{
    int i, j;
    int dumpEndW = x + w;
    int dumpEndH = y + h;
    dumpEndW = (dumpEndW < image->width) ? dumpEndW : image->width;
    dumpEndH = (dumpEndH < image->height) ? dumpEndH : image->height;

    printf("Image[%p] %dx%d %d-bit\n", image, image->width, image->height, image->depth);
    clProfileDebugDump(image->profile);
    for (j = y; j < dumpEndH; ++j) {
        for (i = x; i < dumpEndW; ++i) {
            dumpPixel(image, i, j);
        }
        printf("\n");
    }
}

static void dumpPixel(clImage * image, int x, int y)
{
    COLORIST_ASSERT(image->pixels);
    if (image->depth == 16) {
        uint16_t * shorts = (uint16_t *)image->pixels;
        uint16_t * pixel = &shorts[4 * (x + (y * image->width))];
        printf("    Pixel(%d, %d): (%u, %u, %u, %u)\n", x, y,
            (unsigned int)pixel[0], (unsigned int)pixel[1], (unsigned int)pixel[2], (unsigned int)pixel[3]);
    } else {
        COLORIST_ASSERT(image->depth == 8);
        uint8_t * pixel = &image->pixels[4 * (x + (y * image->width))];
        printf("    Pixel(%d, %d): (%u, %u, %u, %u)\n", x, y,
            (unsigned int)pixel[0], (unsigned int)pixel[1], (unsigned int)pixel[2], (unsigned int)pixel[3]);
    }
}
