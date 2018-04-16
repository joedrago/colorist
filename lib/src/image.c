// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include <string.h>

static int depthToBytes(clContext * C, int depth);

clImage * clImageCreate(clContext * C, int width, int height, int depth, clProfile * profile)
{
    clImage * image = clAllocateStruct(clImage);
    image->profile = profile;
    if (image->profile) {
        image->profile = clProfileClone(C, profile);
    } else {
        image->profile = clProfileCreateStock(C, CL_PS_SRGB);
    }
    clImageResize(C, image, width, height, depth);
    return image;
}

void clImageResize(clContext * C, clImage * image, int width, int height, int depth)
{
    uint8_t * prevPixels = NULL;
    if ((image->width == width) && (image->height == height) && (image->depth == depth)) {
        return;
    }

    if (image->pixels) {
        if ((image->width != width) || (image->height != height)) {
            COLORIST_WARNING("Image resize is unsupported, throwing out pixel data");
            clFree(image->pixels);
            image->pixels = NULL;
        } else {
            prevPixels = image->pixels;
        }
    }

    image->width = width;
    image->height = height;
    image->depth = depth;
    image->size = 4 * image->width * image->height * depthToBytes(C, image->depth);
    image->pixels = (uint8_t *)clAllocate(image->size);

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
        clFree(prevPixels);
    } else {
        memset(image->pixels, 0xff, image->size);
    }
}

void clImageChangeDepth(clContext * C, clImage * image, int depth)
{
    clImageResize(C, image, image->width, image->height, depth);
}

void clImageSetPixel(clContext * C, clImage * image, int x, int y, int r, int g, int b, int a)
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

clImage * clImageRotate(struct clContext * C, clImage * image, int cwTurns)
{
    clImage * rotated;
    int i, j;
    int pixelBytes = depthToBytes(C, image->depth) * 4;
    switch (cwTurns) {
        case 0: // Not rotated
            rotated = clImageCreate(C, image->width, image->height, image->depth, image->profile);
            memcpy(rotated->pixels, image->pixels, rotated->size);
            break;
        case 1: // 270 degrees clockwise
            rotated = clImageCreate(C, image->height, image->width, image->depth, image->profile);
            for (j = 0; j < image->height; ++j) {
                for (i = 0; i < image->width; ++i) {
                    uint8_t * srcPixel = &image->pixels[pixelBytes * (i + (j * image->width))];
                    uint8_t * dstPixel = &rotated->pixels[pixelBytes * ((rotated->width - 1 - j) + (i * rotated->width))];
                    memcpy(dstPixel, srcPixel, pixelBytes);
                }
            }
            break;
        case 2: // 180 degrees clockwise
            rotated = clImageCreate(C, image->width, image->height, image->depth, image->profile);
            for (j = 0; j < image->height; ++j) {
                for (i = 0; i < image->width; ++i) {
                    uint8_t * srcPixel = &image->pixels[pixelBytes * (i + (j * image->width))];
                    uint8_t * dstPixel = &rotated->pixels[pixelBytes * ((rotated->width - 1 - i) + ((rotated->height - 1 - j) * rotated->width))];
                    memcpy(dstPixel, srcPixel, pixelBytes);
                }
            }
            break;
        case 3: // 270 degrees clockwise
            rotated = clImageCreate(C, image->height, image->width, image->depth, image->profile);
            for (j = 0; j < image->height; ++j) {
                for (i = 0; i < image->width; ++i) {
                    uint8_t * srcPixel = &image->pixels[pixelBytes * (i + (j * image->width))];
                    uint8_t * dstPixel = &rotated->pixels[pixelBytes * (j + (i * rotated->width))];
                    memcpy(dstPixel, srcPixel, pixelBytes);
                }
            }
            break;
    }
    return rotated;
}

void clImageDestroy(clContext * C, clImage * image)
{
    clProfileDestroy(C, image->profile);
    if (image->pixels) {
        clFree(image->pixels);
    }
    clFree(image);
}

static int depthToBytes(clContext * C, int depth)
{
    switch (depth) {
        case 8: return 1;
        case 16: return 2;
    }
    COLORIST_FAILURE1("unexpected depth: %d", depth);
    return 1;
}
