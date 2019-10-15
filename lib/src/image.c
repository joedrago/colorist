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

static uint8_t * clImagePixelPtr(clContext * C, clImage * image, clPixelFormat pixelFormat)
{
    COLORIST_UNUSED(C);
    switch (pixelFormat) {
        case CL_PIXELFORMAT_U8:
            return image->pixelsU8;
        case CL_PIXELFORMAT_U16:
            return (uint8_t *)image->pixelsU16;
        case CL_PIXELFORMAT_F32:
            return (uint8_t *)image->pixelsF32;
        case CL_PIXELFORMAT_COUNT:
            COLORIST_ASSERT(0);
            break;
    }
    return NULL;
}

static void clImageAllocatePixels(struct clContext * C, clImage * image, clPixelFormat pixelFormat)
{
    switch (pixelFormat) {
        case CL_PIXELFORMAT_U8:
            if (!image->pixelsU8) {
                image->pixelsU8 = clAllocate(image->width * image->height * CL_BYTES_PER_PIXEL(pixelFormat));
            }
            break;
        case CL_PIXELFORMAT_U16:
            if (!image->pixelsU16) {
                image->pixelsU16 = clAllocate(image->width * image->height * CL_BYTES_PER_PIXEL(pixelFormat));
            }
            break;
        case CL_PIXELFORMAT_F32:
            if (!image->pixelsF32) {
                image->pixelsF32 = clAllocate(image->width * image->height * CL_BYTES_PER_PIXEL(pixelFormat));
            }
            break;
        case CL_PIXELFORMAT_COUNT:
            COLORIST_ASSERT(0);
            break;
    }
}

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
    image->pixelsU8 = NULL;
    image->pixelsU16 = NULL;
    image->pixelsF32 = NULL;
    return image;
}

void clImagePrepareReadPixels(struct clContext * C, clImage * image, clPixelFormat pixelFormat)
{
    static const uint32_t maxChannelU8 = 255;
    static const float maxChannelU8f = 255.0f;
    uint32_t maxChannelU16 = (1 << image->depth) - 1;
    float maxChannelU16f = (float)maxChannelU16;

    switch (pixelFormat) {
        case CL_PIXELFORMAT_U8:
            if (!image->pixelsU8) {
                clImageAllocatePixels(C, image, pixelFormat);

                if (image->pixelsF32) {
                    // F32 -> U8
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            float * srcPixel = &image->pixelsF32[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            uint8_t * dstPixel = &image->pixelsU8[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            dstPixel[0] = (uint8_t)clPixelMathRoundUNorm(srcPixel[0], maxChannelU8);
                            dstPixel[1] = (uint8_t)clPixelMathRoundUNorm(srcPixel[1], maxChannelU8);
                            dstPixel[2] = (uint8_t)clPixelMathRoundUNorm(srcPixel[2], maxChannelU8);
                            dstPixel[3] = (uint8_t)clPixelMathRoundUNorm(srcPixel[3], maxChannelU8);
                        }
                    }
                } else if (image->pixelsU16) {
                    // U16 -> U8
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            uint16_t * srcPixel = &image->pixelsU16[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            uint8_t * dstPixel = &image->pixelsU8[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            dstPixel[0] = (uint8_t)clPixelMathRoundUNorm(srcPixel[0] / maxChannelU16f, maxChannelU8);
                            dstPixel[1] = (uint8_t)clPixelMathRoundUNorm(srcPixel[1] / maxChannelU16f, maxChannelU8);
                            dstPixel[2] = (uint8_t)clPixelMathRoundUNorm(srcPixel[2] / maxChannelU16f, maxChannelU8);
                            dstPixel[3] = (uint8_t)clPixelMathRoundUNorm(srcPixel[3] / maxChannelU16f, maxChannelU8);
                        }
                    }
                } else {
                    // U8 White
                    memset(image->pixelsU8, 0xff, image->width * image->height * sizeof(uint8_t));
                }
            }
            break;

        case CL_PIXELFORMAT_U16:
            if (!image->pixelsU16) {
                clImageAllocatePixels(C, image, pixelFormat);

                if (image->pixelsF32) {
                    // F32 -> U16
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            float * srcPixel = &image->pixelsF32[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            uint16_t * dstPixel = &image->pixelsU16[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            dstPixel[0] = (uint16_t)clPixelMathRoundUNorm(srcPixel[0], maxChannelU16);
                            dstPixel[1] = (uint16_t)clPixelMathRoundUNorm(srcPixel[1], maxChannelU16);
                            dstPixel[2] = (uint16_t)clPixelMathRoundUNorm(srcPixel[2], maxChannelU16);
                            dstPixel[3] = (uint16_t)clPixelMathRoundUNorm(srcPixel[3], maxChannelU16);
                        }
                    }
                } else if (image->pixelsU8) {
                    // U8 -> U16
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            uint8_t * srcPixel = &image->pixelsU8[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            uint16_t * dstPixel = &image->pixelsU16[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            dstPixel[0] = (uint16_t)clPixelMathRoundUNorm(srcPixel[0] / maxChannelU8f, maxChannelU16);
                            dstPixel[1] = (uint16_t)clPixelMathRoundUNorm(srcPixel[1] / maxChannelU8f, maxChannelU16);
                            dstPixel[2] = (uint16_t)clPixelMathRoundUNorm(srcPixel[2] / maxChannelU8f, maxChannelU16);
                            dstPixel[3] = (uint16_t)clPixelMathRoundUNorm(srcPixel[3] / maxChannelU8f, maxChannelU16);
                        }
                    }
                } else {
                    // U16 White
                    memset(image->pixelsU16, 0xff, image->width * image->height * sizeof(uint16_t));
                }
            }
            break;

        case CL_PIXELFORMAT_F32:
            if (!image->pixelsF32) {
                clImageAllocatePixels(C, image, pixelFormat);

                if (image->pixelsU16) {
                    // U16 -> F32
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            uint16_t * srcPixel = &image->pixelsU16[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            float * dstPixel = &image->pixelsF32[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            dstPixel[0] = srcPixel[0] / maxChannelU16f;
                            dstPixel[1] = srcPixel[1] / maxChannelU16f;
                            dstPixel[2] = srcPixel[2] / maxChannelU16f;
                            dstPixel[3] = srcPixel[3] / maxChannelU16f;
                        }
                    }
                } else if (image->pixelsU8) {
                    // U8 -> F32
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            uint8_t * srcPixel = &image->pixelsU8[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            float * dstPixel = &image->pixelsF32[(i + (j * image->width)) * CL_CHANNELS_PER_PIXEL];
                            dstPixel[0] = srcPixel[0] / maxChannelU8f;
                            dstPixel[1] = srcPixel[1] / maxChannelU8f;
                            dstPixel[2] = srcPixel[2] / maxChannelU8f;
                            dstPixel[3] = srcPixel[3] / maxChannelU8f;
                        }
                    }
                } else {
                    // F32 White
                    uint32_t channelCount = image->width * image->height * CL_CHANNELS_PER_PIXEL;
                    for (uint32_t i = 0; i < channelCount; ++i) {
                        image->pixelsF32[i] = 1.0f;
                    }
                }
            }
            break;
    }
}

void clImagePrepareWritePixels(struct clContext * C, clImage * image, clPixelFormat pixelFormat)
{
    clImagePrepareReadPixels(C, image, pixelFormat);

    // Throw away anything that isn't about to be written to; it will be stale and can be repopulated
    // lazily by a future call to clImagePrepareReadPixels().
    if (image->pixelsU8 && (pixelFormat != CL_PIXELFORMAT_U8)) {
        clFree(image->pixelsU8);
        image->pixelsU8 = NULL;
    }
    if (image->pixelsU16 && (pixelFormat != CL_PIXELFORMAT_U16)) {
        clFree(image->pixelsU16);
        image->pixelsU16 = NULL;
    }
    if (image->pixelsF32 && (pixelFormat != CL_PIXELFORMAT_F32)) {
        clFree(image->pixelsF32);
        image->pixelsF32 = NULL;
    }
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
    for (clPixelFormat pixelFormat = CL_PIXELFORMAT_FIRST; pixelFormat != CL_PIXELFORMAT_COUNT; ++pixelFormat) {
        uint8_t * srcPixels = clImagePixelPtr(C, srcImage, pixelFormat);
        if (!srcPixels) {
            continue;
        }
        clImageAllocatePixels(C, dstImage, pixelFormat);
        uint8_t * dstPixels = clImagePixelPtr(C, dstImage, pixelFormat);
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                uint8_t * src = &srcPixels[CL_BYTES_PER_PIXEL(pixelFormat) * ((i + x) + (srcImage->width * (j + y)))];
                uint8_t * dst = &dstPixels[CL_BYTES_PER_PIXEL(pixelFormat) * (i + (dstImage->width * j))];
                memcpy(dst, src, CL_BYTES_PER_PIXEL(pixelFormat));
            }
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

    clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_F32);
    clImagePrepareReadPixels(C, hald, CL_PIXELFORMAT_F32);
    clImagePrepareWritePixels(C, appliedImage, CL_PIXELFORMAT_F32);

    int pixelCount = image->width * image->height;
    for (int i = 0; i < pixelCount; ++i) {
        clPixelMathHaldCLUTLookup(C,
                                  hald->pixelsF32,
                                  haldDims,
                                  &image->pixelsF32[i * CL_CHANNELS_PER_PIXEL],
                                  &appliedImage->pixelsF32[i * CL_CHANNELS_PER_PIXEL]);
    }

    return appliedImage;
}

clImage * clImageResize(struct clContext * C, clImage * image, int width, int height, clFilter resizeFilter)
{
    clImage * resizedImage = clImageCreate(C, width, height, image->depth, image->profile);

    clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_F32);
    clImagePrepareWritePixels(C, resizedImage, CL_PIXELFORMAT_F32);

    clPixelMathResize(C, image->width, image->height, image->pixelsF32, resizedImage->width, resizedImage->height, resizedImage->pixelsF32, resizeFilter);
    int resizedChannelCount = resizedImage->width * resizedImage->height * CL_CHANNELS_PER_PIXEL;
    for (int i = 0; i < resizedChannelCount; ++i) {
        // catmullrom and mitchell sometimes give values outside of 0-1, so clamp them
        resizedImage->pixelsF32[i] = CL_CLAMP(resizedImage->pixelsF32[i], 0.0f, 1.0f);
    }
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
    clTransform * srcBlendTransform = clTransformCreate(C, image->profile, CL_XF_RGBA, blendProfile, CL_XF_RGBA, blendParams->srcTonemap);
    clTransform * cmpBlendTransform =
        clTransformCreate(C, compositeImage->profile, CL_XF_RGBA, blendProfile, CL_XF_RGBA, blendParams->cmpTonemap);
    clTransform * dstTransform =
        clTransformCreate(C, blendProfile, CL_XF_RGBA, image->profile, CL_XF_RGBA, CL_TONEMAP_OFF); // maxLuminance should match, no need to tonemap

    // Transform src and comp images into normalized blend space
    clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_F32);
    clImagePrepareReadPixels(C, compositeImage, CL_PIXELFORMAT_F32);
    int pixelCount = image->width * image->height;
    float * srcFloats = clAllocate(4 * sizeof(float) * pixelCount);
    clTransformRun(C, srcBlendTransform, image->pixelsF32, srcFloats, pixelCount);
    float * cmpFloats = clAllocate(4 * sizeof(float) * pixelCount);
    clTransformRun(C, cmpBlendTransform, compositeImage->pixelsF32, cmpFloats, pixelCount);

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
    clImagePrepareWritePixels(C, dstImage, CL_PIXELFORMAT_F32);
    clTransformRun(C, dstTransform, dstFloats, dstImage->pixelsF32, pixelCount);

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

clImage * clImageRotate(struct clContext * C, clImage * image, int cwTurns)
{
    clImage * rotated = NULL;

    switch (cwTurns) {
        case 0: // Not rotated
            rotated = clImageCreate(C, image->width, image->height, image->depth, image->profile);
            break;
        case 1: // 90 degrees clockwise
            rotated = clImageCreate(C, image->height, image->width, image->depth, image->profile);
            break;
        case 2: // 180 degrees clockwise
            rotated = clImageCreate(C, image->width, image->height, image->depth, image->profile);
            break;
        case 3: // 270 degrees clockwise
            rotated = clImageCreate(C, image->height, image->width, image->depth, image->profile);
            break;
    }

    if (rotated) {
        for (clPixelFormat pixelFormat = CL_PIXELFORMAT_FIRST; pixelFormat != CL_PIXELFORMAT_COUNT; ++pixelFormat) {
            uint8_t * srcPixels = clImagePixelPtr(C, image, pixelFormat);
            if (!srcPixels) {
                continue;
            }
            clImageAllocatePixels(C, rotated, pixelFormat);
            uint8_t * dstPixels = clImagePixelPtr(C, rotated, pixelFormat);

            switch (cwTurns) {
                case 0: // Not rotated
                    memcpy(dstPixels, srcPixels, rotated->width * rotated->height * CL_BYTES_PER_PIXEL(pixelFormat));
                    break;
                case 1: // 90 degrees clockwise
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            uint8_t * srcPixel = &srcPixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                            uint8_t * dstPixel = &dstPixels[CL_CHANNELS_PER_PIXEL * ((rotated->width - 1 - j) + (i * rotated->width))];
                            memcpy(dstPixel, srcPixel, CL_BYTES_PER_PIXEL(pixelFormat));
                        }
                    }
                    break;
                case 2: // 180 degrees clockwise
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            uint8_t * srcPixel = &srcPixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                            uint8_t * dstPixel = &dstPixels[CL_CHANNELS_PER_PIXEL * ((rotated->width - 1 - i) +
                                                                                     ((rotated->height - 1 - j) * rotated->width))];
                            memcpy(dstPixel, srcPixel, CL_BYTES_PER_PIXEL(pixelFormat));
                        }
                    }
                    break;
                case 3: // 270 degrees clockwise
                    for (int j = 0; j < image->height; ++j) {
                        for (int i = 0; i < image->width; ++i) {
                            uint8_t * srcPixel = &srcPixels[CL_CHANNELS_PER_PIXEL * (i + (j * image->width))];
                            uint8_t * dstPixel = &dstPixels[CL_CHANNELS_PER_PIXEL * (j + ((rotated->height - 1 - i) * rotated->width))];
                            memcpy(dstPixel, srcPixel, CL_BYTES_PER_PIXEL(pixelFormat));
                        }
                    }
                    break;
            }
        }
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
    clTransform * transform = clTransformCreate(C, srcImage->profile, CL_XF_RGBA, dstImage->profile, CL_XF_RGBA, tonemap);
    clTransformPrepare(C, transform);
    float luminanceScale = clTransformGetLuminanceScale(C, transform);

    clImagePrepareReadPixels(C, srcImage, CL_PIXELFORMAT_F32);
    clImagePrepareWritePixels(C, dstImage, CL_PIXELFORMAT_F32);

    // Perform conversion
    clContextLog(C,
                 "convert",
                 0,
                 "Converting (%s, lum scale %gx, %s)...",
                 clTransformCMMName(C, transform),
                 luminanceScale,
                 transform->tonemapEnabled ? "tonemap" : "clip");
    timerStart(&t);
    clTransformRun(C, transform, srcImage->pixelsF32, dstImage->pixelsF32, srcImage->width * srcImage->height);
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
    clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_F32);
    clPixelMathColorGrade(C, image->profile, image->pixelsF32, pixelCount, image->width, srcLuminance, dstColorDepth, outLuminance, outGamma, verbose);
}

void clImageDestroy(clContext * C, clImage * image)
{
    clProfileDestroy(C, image->profile);
    if (image->pixelsU8) {
        clFree(image->pixelsU8);
    }
    if (image->pixelsU16) {
        clFree(image->pixelsU16);
    }
    if (image->pixelsF32) {
        clFree(image->pixelsF32);
    }
    clFree(image);
}
