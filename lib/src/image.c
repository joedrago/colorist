// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include <string.h>

void clImageLogCreate(clContext * C, int width, int height, int depth, clProfile * profile)
{
    COLORIST_UNUSED(width);
    COLORIST_UNUSED(height);
    COLORIST_UNUSED(depth);

    if (profile == NULL) {
        clContextLog(C, "decode", 1, "No embedded ICC profile, using SRGB");
    }
}

clImage * clImageCreate(clContext * C, int width, int height, int depth, clProfile * profile)
{
    clImage * image = clAllocateStruct(clImage);
    image->profile = profile;
    if (image->profile) {
        image->profile = clProfileClone(C, profile);
    } else {
        image->profile = clProfileCreateStock(C, CL_PS_SRGB);
    }
    image->width = width;
    image->height = height;
    image->depth = depth;
    image->size = image->width * image->height * CL_BYTES_PER_PIXEL;
    image->pixels = (uint16_t *)clAllocate(image->size);
    memset(image->pixels, 0xff, image->size);
    return image;
}

clImage * clImageCrop(struct clContext * C, clImage * srcImage, int x, int y, int w, int h, clBool keepSrc)
{
    if (!srcImage) {
        return NULL;
    }
    if (!clImageAdjustRect(C, srcImage, &x, &y, &w, &h)) {
        return NULL;
    }

    clImage * dstImage = clImageCreate(C, w, h, srcImage->depth, srcImage->profile);
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            uint16_t * src = &srcImage->pixels[CL_CHANNELS_PER_PIXEL * ((i + x) + (srcImage->width * (j + y)))];
            uint16_t * dst = &dstImage->pixels[CL_CHANNELS_PER_PIXEL * (i + (dstImage->width * j))];
            memcpy(dst, src, CL_BYTES_PER_PIXEL);
        }
    }

    if (!keepSrc) {
        clImageDestroy(C, srcImage);
    }
    return dstImage;
}

clImage * clImageApplyHALD(struct clContext * C, clImage * image, clImage * hald, int haldDims)
{
    clImage * appliedImage = clImageCreate(C, image->width, image->height, image->depth, image->profile);
    int pixelCount = image->width * image->height;
    int haldDataCount = hald->width * hald->height;

    float * haldData = clAllocate(4 * sizeof(float) * haldDataCount);
    clPixelMathUNormToFloat(C, hald->pixels, hald->depth, haldData, haldDataCount);
    float * srcFloats = clAllocate(4 * sizeof(float) * pixelCount);
    clPixelMathUNormToFloat(C, image->pixels, image->depth, srcFloats, pixelCount);
    float * dstFloats = clAllocate(4 * sizeof(float) * pixelCount);

    for (int i = 0; i < pixelCount; ++i) {
        clPixelMathHaldCLUTLookup(C, haldData, haldDims, &srcFloats[i * 4], &dstFloats[i * 4]);
    }
    clPixelMathFloatToUNorm(C, dstFloats, appliedImage->pixels, appliedImage->depth, pixelCount);

    clFree(dstFloats);
    clFree(srcFloats);
    clFree(haldData);
    return appliedImage;
}

clImage * clImageResize(struct clContext * C, clImage * image, int width, int height, clFilter resizeFilter)
{
    clImage * resizedImage = clImageCreate(C, width, height, image->depth, image->profile);
    int pixelCount = image->width * image->height;
    int resizedPixelCount = resizedImage->width * resizedImage->height;
    float * srcFloats = clAllocate(4 * sizeof(float) * pixelCount);
    float * dstFloats = clAllocate(4 * sizeof(float) * resizedPixelCount);

    clPixelMathUNormToFloat(C, image->pixels, image->depth, srcFloats, pixelCount);
    clPixelMathResize(C, image->width, image->height, srcFloats, resizedImage->width, resizedImage->height, dstFloats, resizeFilter);
    int resizedChannelCount = resizedPixelCount * 4;
    for (int i = 0; i < resizedChannelCount; ++i) {
        // catmullrom and mitchell sometimes give values outside of 0-1, so clamp before calling clPixelMathFloatToUNorm
        dstFloats[i] = CL_CLAMP(dstFloats[i], 0.0f, 1.0f);
    }
    clPixelMathFloatToUNorm(C, dstFloats, resizedImage->pixels, resizedImage->depth, resizedPixelCount);
    clFree(dstFloats);
    clFree(srcFloats);
    return resizedImage;
}

clBool clImageAdjustRect(struct clContext * C, clImage * image, int * x, int * y, int * w, int * h)
{
    COLORIST_UNUSED(C);

    if ((*x < 0) || (*y < 0) || (*w <= 0) || (*h <= 0)) {
        return clFalse;
    }

    *x = (*x < image->width) ? *x : image->width - 1;
    *y = (*y < image->height) ? *y : image->height - 1;

    int endX = *x + *w;
    int endY = *y + *h;
    endX = (endX < image->width) ? endX : image->width;
    endY = (endY < image->height) ? endY : image->height;

    *w = endX - *x;
    *h = endY - *y;
    return clTrue;
}

void clImageSetPixel(clContext * C, clImage * image, int x, int y, int r, int g, int b, int a)
{
    COLORIST_UNUSED(C);

    uint16_t * pixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (x + (y * image->width))];
    pixel[0] = (uint16_t)r;
    pixel[1] = (uint16_t)g;
    pixel[2] = (uint16_t)b;
    pixel[3] = (uint16_t)a;
}

clImage * clImageRotate(struct clContext * C, clImage * image, int cwTurns)
{
    clImage * rotated = NULL;
    switch (cwTurns) {
        case 0: // Not rotated
            rotated = clImageCreate(C, image->width, image->height, image->depth, image->profile);
            memcpy(rotated->pixels, image->pixels, rotated->size);
            break;
        case 1: // 270 degrees clockwise
            rotated = clImageCreate(C, image->height, image->width, image->depth, image->profile);
            for (int j = 0; j < image->height; ++j) {
                for (int i = 0; i < image->width; ++i) {
                    uint16_t * srcPixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                    uint16_t * dstPixel = &rotated->pixels[CL_CHANNELS_PER_PIXEL * ((rotated->width - 1 - j) + (i * rotated->width))];
                    memcpy(dstPixel, srcPixel, CL_BYTES_PER_PIXEL);
                }
            }
            break;
        case 2: // 180 degrees clockwise
            rotated = clImageCreate(C, image->width, image->height, image->depth, image->profile);
            for (int j = 0; j < image->height; ++j) {
                for (int i = 0; i < image->width; ++i) {
                    uint16_t * srcPixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                    uint16_t * dstPixel = &rotated->pixels[CL_CHANNELS_PER_PIXEL * ((rotated->width - 1 - i) + ((rotated->height - 1 - j) * rotated->width))];
                    memcpy(dstPixel, srcPixel, CL_BYTES_PER_PIXEL);
                }
            }
            break;
        case 3: // 270 degrees clockwise
            rotated = clImageCreate(C, image->height, image->width, image->depth, image->profile);
            for (int j = 0; j < image->height; ++j) {
                for (int i = 0; i < image->width; ++i) {
                    uint16_t * srcPixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                    uint16_t * dstPixel = &rotated->pixels[CL_CHANNELS_PER_PIXEL * (j + ((rotated->height - 1 - i) * rotated->width))];
                    memcpy(dstPixel, srcPixel, CL_BYTES_PER_PIXEL);
                }
            }
            break;
    }
    return rotated;
}

clImage * clImageConvert(struct clContext * C, clImage * srcImage, int taskCount, int depth, struct clProfile * dstProfile, clTonemap tonemap)
{
    Timer t;

    // Create destination image
    clImage * dstImage = clImageCreate(C, srcImage->width, srcImage->height, depth, dstProfile);

    // Show image details
    clContextLog(C, "details", 0, "Source:");
    clImageDebugDump(C, srcImage, 0, 0, 0, 0, 1);
    clContextLog(C, "details", 0, "Destination:");
    clImageDebugDump(C, dstImage, 0, 0, 0, 0, 1);

    // Create the transform
    clTransform * transform = clTransformCreate(C, srcImage->profile, CL_XF_RGBA, srcImage->depth, dstImage->profile, CL_XF_RGBA, depth, tonemap);
    clTransformPrepare(C, transform);
    float luminanceScale = clTransformGetLuminanceScale(C, transform);

    // Perform conversion
    clContextLog(C, "convert", 0, "Converting (%s, lum scale %gx, %s)...", clTransformCMMName(C, transform), luminanceScale, transform->tonemapEnabled ? "tonemap" : "clip");
    timerStart(&t);
    clTransformRun(C, transform, taskCount, srcImage->pixels, dstImage->pixels, srcImage->width * srcImage->height);
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    // Cleanup
    clTransformDestroy(C, transform);
    return dstImage;
}

void clImageColorGrade(struct clContext * C, clImage * image, int taskCount, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose)
{
    int srcLuminance = 0;
    clProfileQuery(C, image->profile, NULL, NULL, &srcLuminance);
    srcLuminance = (srcLuminance != 0) ? srcLuminance : COLORIST_DEFAULT_LUMINANCE;

    int pixelCount = image->width * image->height;
    float * floatPixels = clAllocate(4 * sizeof(float) * pixelCount);
    clPixelMathUNormToFloat(C, image->pixels, image->depth, floatPixels, pixelCount);
    clPixelMathColorGrade(C, taskCount, image->profile, floatPixels, pixelCount, image->width, srcLuminance, dstColorDepth, outLuminance, outGamma, verbose);
    clFree(floatPixels);
}

void clImageDestroy(clContext * C, clImage * image)
{
    clProfileDestroy(C, image->profile);
    if (image->pixels) {
        clFree(image->pixels);
    }
    clFree(image);
}

void clImageToRGB8(struct clContext * C, clImage * image, uint8_t * outPixels)
{
    COLORIST_UNUSED(C);

    if (image->depth == 8) {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * srcPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint8_t * dstPixel = &outPixels[(i + (j * image->width)) * 3];
                dstPixel[0] = (uint8_t)srcPixel[0];
                dstPixel[1] = (uint8_t)srcPixel[1];
                dstPixel[2] = (uint8_t)srcPixel[2];
            }
        }
    } else {
        float maxSrcChannel = (float)((1 << image->depth) - 1);
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * srcPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint8_t * dstPixel = &outPixels[(i + (j * image->width)) * 3];
                dstPixel[0] = (uint8_t)clPixelMathRoundf((srcPixel[0] / maxSrcChannel) * 255.0f);
                dstPixel[1] = (uint8_t)clPixelMathRoundf((srcPixel[1] / maxSrcChannel) * 255.0f);
                dstPixel[2] = (uint8_t)clPixelMathRoundf((srcPixel[2] / maxSrcChannel) * 255.0f);
            }
        }
    }
}

void clImageFromRGB8(struct clContext * C, clImage * image, uint8_t * inPixels)
{
    COLORIST_UNUSED(C);

    if (image->depth == 8) {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * srcPixel = &inPixels[(i + (j * image->width)) * 3];
                uint16_t * dstPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[0] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[2] = srcPixel[2];
                dstPixel[3] = 255;
            }
        }
    } else {
        uint16_t maxDstChannel = (uint16_t)((1 << image->depth) - 1);
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * srcPixel = &inPixels[(i + (j * image->width)) * 3];
                uint16_t * dstPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[0] = (uint8_t)clPixelMathRoundf((srcPixel[0] / 255.0f) * maxDstChannel);
                dstPixel[1] = (uint8_t)clPixelMathRoundf((srcPixel[1] / 255.0f) * maxDstChannel);
                dstPixel[2] = (uint8_t)clPixelMathRoundf((srcPixel[2] / 255.0f) * maxDstChannel);
                dstPixel[3] = maxDstChannel;
            }
        }
    }
}
void clImageToRGBA8(struct clContext * C, clImage * image, uint8_t * outPixels)
{
    COLORIST_UNUSED(C);

    if (image->depth == 8) {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * srcPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint8_t * dstPixel = &outPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[0] = (uint8_t)srcPixel[0];
                dstPixel[1] = (uint8_t)srcPixel[1];
                dstPixel[2] = (uint8_t)srcPixel[2];
                dstPixel[3] = (uint8_t)srcPixel[3];
            }
        }
    } else {
        float maxSrcChannel = (float)((1 << image->depth) - 1);
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * srcPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint8_t * dstPixel = &outPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[0] = (uint8_t)clPixelMathRoundf((srcPixel[0] / maxSrcChannel) * 255.0f);
                dstPixel[1] = (uint8_t)clPixelMathRoundf((srcPixel[1] / maxSrcChannel) * 255.0f);
                dstPixel[2] = (uint8_t)clPixelMathRoundf((srcPixel[2] / maxSrcChannel) * 255.0f);
                dstPixel[3] = (uint8_t)clPixelMathRoundf((srcPixel[3] / maxSrcChannel) * 255.0f);
            }
        }
    }
}

void clImageFromRGBA8(struct clContext * C, clImage * image, uint8_t * inPixels)
{
    COLORIST_UNUSED(C);

    if (image->depth == 8) {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * srcPixel = &inPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint16_t * dstPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[0] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[2] = srcPixel[2];
                dstPixel[3] = srcPixel[3];
            }
        }
    } else {
        float maxDstChannel = (float)((1 << image->depth) - 1);
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * srcPixel = &inPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint16_t * dstPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[0] = (uint8_t)clPixelMathRoundf((srcPixel[0] / 255.0f) * maxDstChannel);
                dstPixel[1] = (uint8_t)clPixelMathRoundf((srcPixel[1] / 255.0f) * maxDstChannel);
                dstPixel[2] = (uint8_t)clPixelMathRoundf((srcPixel[2] / 255.0f) * maxDstChannel);
                dstPixel[3] = (uint8_t)clPixelMathRoundf((srcPixel[3] / 255.0f) * maxDstChannel);
            }
        }
    }
}

void clImageToBGRA8(struct clContext * C, clImage * image, uint8_t * outPixels)
{
    COLORIST_UNUSED(C);

    if (image->depth == 8) {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * srcPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint8_t * dstPixel = &outPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[2] = (uint8_t)srcPixel[0];
                dstPixel[1] = (uint8_t)srcPixel[1];
                dstPixel[0] = (uint8_t)srcPixel[2];
                dstPixel[3] = (uint8_t)srcPixel[3];
            }
        }
    } else {
        float maxSrcChannel = (float)((1 << image->depth) - 1);
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * srcPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint8_t * dstPixel = &outPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[2] = (uint8_t)clPixelMathRoundf((srcPixel[0] / maxSrcChannel) * 255.0f);
                dstPixel[1] = (uint8_t)clPixelMathRoundf((srcPixel[1] / maxSrcChannel) * 255.0f);
                dstPixel[0] = (uint8_t)clPixelMathRoundf((srcPixel[2] / maxSrcChannel) * 255.0f);
                dstPixel[3] = (uint8_t)clPixelMathRoundf((srcPixel[3] / maxSrcChannel) * 255.0f);
            }
        }
    }
}

void clImageFromBGRA8(struct clContext * C, clImage * image, uint8_t * inPixels)
{
    COLORIST_UNUSED(C);

    if (image->depth == 8) {
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * srcPixel = &inPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint16_t * dstPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[2] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[0] = srcPixel[2];
                dstPixel[3] = srcPixel[3];
            }
        }
    } else {
        float maxDstChannel = (float)((1 << image->depth) - 1);
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * srcPixel = &inPixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                uint16_t * dstPixel = &image->pixels[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                dstPixel[2] = (uint8_t)clPixelMathRoundf((srcPixel[0] / 255.0f) * maxDstChannel);
                dstPixel[1] = (uint8_t)clPixelMathRoundf((srcPixel[1] / 255.0f) * maxDstChannel);
                dstPixel[0] = (uint8_t)clPixelMathRoundf((srcPixel[2] / 255.0f) * maxDstChannel);
                dstPixel[3] = (uint8_t)clPixelMathRoundf((srcPixel[3] / 255.0f) * maxDstChannel);
            }
        }
    }
}
