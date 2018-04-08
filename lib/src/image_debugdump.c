#include "colorist/image.h"

#include "colorist/profile.h"

#include <stdio.h>

#define COLORIST_DUMP_DIM 4

static void dumpPixel(clImage * image, int x, int y);

void clImageDebugDump(clImage * image)
{
    int i, j;
    int dumpW = (image->width < COLORIST_DUMP_DIM) ? image->width : COLORIST_DUMP_DIM;
    int dumpH = (image->height < COLORIST_DUMP_DIM) ? image->height : COLORIST_DUMP_DIM;

    printf("Image[%p] %dx%d %d-bit\n", image, image->width, image->height, image->depth);
    clProfileDebugDump(image->profile);
    for (j = 0; j < dumpH; ++j) {
        for (i = 0; i < dumpW; ++i) {
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
