// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#define COLORIST_DUMP_DIM 4

static void dumpPixel(struct clContext * C, clImage * image, int x, int y, int extraIndent);

void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent)
{
    int i, j;
    int dumpEndW = x + w;
    int dumpEndH = y + h;
    dumpEndW = (dumpEndW < image->width) ? dumpEndW : image->width;
    dumpEndH = (dumpEndH < image->height) ? dumpEndH : image->height;

    clContextLog(C, "image", 0 + extraIndent, "Image: %dx%d %d-bit", image->width, image->height, image->depth);
    clProfileDebugDump(C, image->profile, 1 + extraIndent);
    if (((dumpEndW - x) > 0) && ((dumpEndH - y) > 0)) {
        clContextLog(C, "image", 1 + extraIndent, "Pixels:");
    }
    for (j = y; j < dumpEndH; ++j) {
        for (i = x; i < dumpEndW; ++i) {
            dumpPixel(C, image, i, j, extraIndent);
        }
        // clContextLog(C, "image", 0 + extraIndent, "");
    }
}

static void dumpPixel(struct clContext * C, clImage * image, int x, int y, int extraIndent)
{
    COLORIST_ASSERT(image->pixels);
    if (image->depth == 16) {
        uint16_t * shorts = (uint16_t *)image->pixels;
        uint16_t * pixel = &shorts[4 * (x + (y * image->width))];
        clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): (%u, %u, %u, %u) -> (%g, %g, %g, %g)",
            x, y,
            (unsigned int)pixel[0], (unsigned int)pixel[1], (unsigned int)pixel[2], (unsigned int)pixel[3],
            (float)pixel[0] / 65535.0f, (float)pixel[1] / 65535.0f, (float)pixel[2] / 65535.0f, (float)pixel[3] / 65535.0f);
    } else {
        uint8_t * pixel = &image->pixels[4 * (x + (y * image->width))];
        COLORIST_ASSERT(image->depth == 8);
        clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): (%u, %u, %u, %u) -> (%g, %g, %g, %g)",
            x, y,
            (unsigned int)pixel[0], (unsigned int)pixel[1], (unsigned int)pixel[2], (unsigned int)pixel[3],
            (float)pixel[0] / 255.0f, (float)pixel[1] / 255.0f, (float)pixel[2] / 255.0f, (float)pixel[3] / 255.0f);
    }
}
