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

void clBlendParamsSetDefaults(struct clContext * C, clBlendParams * blendParams)
{
    COLORIST_UNUSED(C);

    blendParams->gamma = 2.2f;
    blendParams->srcTonemap = CL_TONEMAP_AUTO;
    blendParams->cmpTonemap = CL_TONEMAP_AUTO;
    blendParams->premultiplied = clFalse;
}

clImage * clImageBlend(struct clContext * C, clImage * image, clImage * compositeImage, clBlendParams * blendParams)
{
    // Sanity checks
    if ((image->width != compositeImage->width) || (image->height != compositeImage->height)) {
        return NULL;
    }

    // Query profile used for both src and dst image
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;
    if (!clProfileQuery(C, image->profile, &primaries, &curve, &maxLuminance)) {
        clContextLogError(C, "clImageBlend: failed to query source profile");
        return NULL;
    }
    maxLuminance = (int)((float)maxLuminance * curve.implicitScale);

    // Build a profile using the same color volume, but a blend-friendly gamma
    curve.type = CL_PCT_GAMMA;
    curve.implicitScale = 1.0f;
    curve.gamma = blendParams->gamma;
    clProfile * blendProfile = clProfileCreate(C, &primaries, &curve, maxLuminance, NULL);

    // Build transforms that go [src -> blend], [cmp -> blend], [blend -> dst]
    clTransform * srcBlendTransform =
        clTransformCreate(C, image->profile, CL_XF_RGBA, image->depth, blendProfile, CL_XF_RGBA, 32, blendParams->srcTonemap);
    clTransform * cmpBlendTransform =
        clTransformCreate(C, compositeImage->profile, CL_XF_RGBA, compositeImage->depth, blendProfile, CL_XF_RGBA, 32, blendParams->cmpTonemap);
    clTransform * dstTransform =
        clTransformCreate(C, blendProfile, CL_XF_RGBA, 32, image->profile, CL_XF_RGBA, image->depth, CL_TONEMAP_OFF); // maxLuminance should match, no need to tonemap

    // Transform src and comp images into normalized blend space
    int pixelCount = image->width * image->height;
    float * srcFloats = clAllocate(4 * sizeof(float) * pixelCount);
    clTransformRun(C, srcBlendTransform, image->pixels, srcFloats, pixelCount);
    float * cmpFloats = clAllocate(4 * sizeof(float) * pixelCount);
    clTransformRun(C, cmpBlendTransform, compositeImage->pixels, cmpFloats, pixelCount);

    // Perform SourceOver blend
    float * dstFloats = clAllocate(4 * sizeof(float) * pixelCount);
    if (blendParams->premultiplied) {
        // Premultiplied alpha
        for (int i = 0; i < pixelCount; ++i) {
            float * srcPixel = &srcFloats[i * 4];
            float * cmpPixel = &cmpFloats[i * 4];
            float * dstPixel = &dstFloats[i * 4];

            // cmpPixel is the "Source" in a SourceOver Porter/Duff blend
            dstPixel[0] = cmpPixel[0] + (srcPixel[0] * (1 - cmpPixel[3]));
            dstPixel[1] = cmpPixel[1] + (srcPixel[1] * (1 - cmpPixel[3]));
            dstPixel[2] = cmpPixel[2] + (srcPixel[2] * (1 - cmpPixel[3]));
            dstPixel[3] = cmpPixel[3] + (srcPixel[3] * (1 - cmpPixel[3]));
        }
    } else {
        // Not Premultiplied alpha, perform the multiply during the blend
        for (int i = 0; i < pixelCount; ++i) {
            float * srcPixel = &srcFloats[i * 4];
            float * cmpPixel = &cmpFloats[i * 4];
            float * dstPixel = &dstFloats[i * 4];

            // cmpPixel is the "Source" in a SourceOver Porter/Duff blend
            dstPixel[0] = (cmpPixel[0] * cmpPixel[3]) + (srcPixel[0] * srcPixel[3] * (1 - cmpPixel[3]));
            dstPixel[1] = (cmpPixel[1] * cmpPixel[3]) + (srcPixel[1] * srcPixel[3] * (1 - cmpPixel[3]));
            dstPixel[2] = (cmpPixel[2] * cmpPixel[3]) + (srcPixel[2] * srcPixel[3] * (1 - cmpPixel[3]));
            dstPixel[3] = cmpPixel[3] + (srcPixel[3] * (1 - cmpPixel[3]));
        }
    }

    // Transform blended pixels into new destination image
    clImage * dstImage = clImageCreate(C, image->width, image->height, image->depth, image->profile);
    clTransformRun(C, dstTransform, dstFloats, dstImage->pixels, pixelCount);

    // Cleanup
    clTransformDestroy(C, srcBlendTransform);
    clTransformDestroy(C, cmpBlendTransform);
    clTransformDestroy(C, dstTransform);
    clProfileDestroy(C, blendProfile);
    clFree(srcFloats);
    clFree(cmpFloats);
    clFree(dstFloats);
    return dstImage;
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
        case 1: // 90 degrees clockwise
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
                    uint16_t * dstPixel =
                        &rotated->pixels[CL_CHANNELS_PER_PIXEL * ((rotated->width - 1 - i) + ((rotated->height - 1 - j) * rotated->width))];
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

clImage * clImageConvert(struct clContext * C, clImage * srcImage, int depth, struct clProfile * dstProfile, clTonemap tonemap)
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
    clTransform * transform =
        clTransformCreate(C, srcImage->profile, CL_XF_RGBA, srcImage->depth, dstImage->profile, CL_XF_RGBA, depth, tonemap);
    clTransformPrepare(C, transform);
    float luminanceScale = clTransformGetLuminanceScale(C, transform);

    // Perform conversion
    clContextLog(C,
                 "convert",
                 0,
                 "Converting (%s, lum scale %gx, %s)...",
                 clTransformCMMName(C, transform),
                 luminanceScale,
                 transform->tonemapEnabled ? "tonemap" : "clip");
    timerStart(&t);
    clTransformRun(C, transform, srcImage->pixels, dstImage->pixels, srcImage->width * srcImage->height);
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    // Cleanup
    clTransformDestroy(C, transform);
    return dstImage;
}

void clImageColorGrade(struct clContext * C, clImage * image, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose)
{
    int srcLuminance = 0;
    clProfileQuery(C, image->profile, NULL, NULL, &srcLuminance);
    srcLuminance = (srcLuminance != 0) ? srcLuminance : C->defaultLuminance;

    int pixelCount = image->width * image->height;
    float * floatPixels = clAllocate(4 * sizeof(float) * pixelCount);
    clPixelMathUNormToFloat(C, image->pixels, image->depth, floatPixels, pixelCount);
    clPixelMathColorGrade(C, image->profile, floatPixels, pixelCount, image->width, srcLuminance, dstColorDepth, outLuminance, outGamma, verbose);
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
