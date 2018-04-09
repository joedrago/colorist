// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/profile.h"

#include <string.h>

static int depthToBytes(int depth);

clImage * clImageCreate(int width, int height, int depth, clProfile * profile)
{
    clImage * image = clAllocate(clImage);
    image->profile = profile;
    if (image->profile) {
        image->profile = clProfileClone(profile);
    } else {
        image->profile = clProfileCreateStock(CL_PS_SRGB);
    }
    clImageResize(image, width, height, depth);
    return image;
}

void clImageResize(clImage * image, int width, int height, int depth)
{
    uint8_t * prevPixels = NULL;
    if ((image->width == width) && (image->height == height) && (image->depth == depth)) {
        return;
    }

    if (image->pixels) {
        if ((image->width != width) || (image->height != height)) {
            COLORIST_WARNING("Image resize is unsupported, throwing out pixel data");
            free(image->pixels);
            image->pixels = NULL;
        } else {
            prevPixels = image->pixels;
        }
    }

    image->width = width;
    image->height = height;
    image->depth = depth;
    image->size = 4 * image->width * image->height * depthToBytes(image->depth);
    image->pixels = (uint8_t *)malloc(image->size);

    if (prevPixels) {
        // If we get here, we're trying to depth shift
        const int channelCount = 4 * image->width * image->height;
        int i;
        if (image->depth == 16) {
            uint8_t * src = prevPixels;
            uint16_t * dst = (uint16_t *)image->pixels;
            for (i = 0; i < channelCount; ++i) {
                dst[i] = (uint16_t)((float)src[i] / 255.0f * 65535.0f);
            }
        } else {
            uint16_t * src = (uint16_t *)prevPixels;
            uint8_t * dst = image->pixels;
            COLORIST_ASSERT(image->depth == 8);
            for (i = 0; i < channelCount; ++i) {
                dst[i] = src[i] >> 8;
            }
        }
        free(prevPixels);
    } else {
        memset(image->pixels, 0xff, image->size);
    }
}

void clImageChangeDepth(clImage * image, int depth)
{
    clImageResize(image, image->width, image->height, depth);
}

void clImageSetPixel(clImage * image, int x, int y, int r, int g, int b, int a)
{
    if (image->depth == 16) {
        uint16_t * pixels = (uint16_t *)image->pixels;
        uint16_t * pixel = &pixels[4 * (x + (y * image->width))];
        pixel[0] = (uint16_t)r;
        pixel[1] = (uint16_t)g;
        pixel[2] = (uint16_t)b;
        pixel[3] = (uint16_t)a;
    } else {
        uint8_t * pixels = image->pixels;
        uint8_t * pixel = &pixels[4 * (x + (y * image->width))];
        COLORIST_ASSERT(image->depth == 8);
        pixel[0] = (uint8_t)r;
        pixel[1] = (uint8_t)g;
        pixel[2] = (uint8_t)b;
        pixel[3] = (uint8_t)a;
    }
}

void clImageDestroy(clImage * image)
{
    if (image->pixels) {
        free(image->pixels);
    }
    free(image);
}

static int depthToBytes(int depth)
{
    switch (depth) {
        case  8: return 1;
        case 16: return 2;
    }
    COLORIST_FAILURE1("unexpected depth: %d", depth);
    return 1;
}
