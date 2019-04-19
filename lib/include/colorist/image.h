// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_IMAGE_H
#define COLORIST_IMAGE_H

#include "colorist/context.h"
#include "colorist/types.h"

#define CL_CHANNELS_PER_PIXEL 4 // R, G, B, A
#define CL_BYTES_PER_CHANNEL 2
#define CL_BYTES_PER_PIXEL (CL_CHANNELS_PER_PIXEL * CL_BYTES_PER_CHANNEL)

struct clProfile;
struct clRaw;
struct cJSON;

typedef struct clImage
{
    int width;
    int height;
    int depth;
    int size;
    uint16_t * pixels; // always RGBA
    struct clProfile * profile;
} clImage;

typedef struct clImageSignals
{
    float mseLinear;
    float psnrLinear;
    float mseG22;
    float psnrG22;
} clImageSignals;

typedef struct clImagePixelInfo
{
    // Raw value in pixel data
    uint16_t rawR;
    uint16_t rawG;
    uint16_t rawB;
    uint16_t rawA;

    // floating point normalized 0-1
    float normR;
    float normG;
    float normB;
    float normA;

    // XYZ
    float X;
    float Y;
    float Z;

    // xyY
    float x;
    float y;
    // Y is the same from XYZ above

    float nits;
} clImagePixelInfo;

typedef struct clImageDiff
{
    clImage * image;
    uint16_t * diffs;
    uint16_t * intensities;
    float minIntensity;
    int pixelCount;
    int matchCount;
    int underThresholdCount;
    int overThresholdCount;
    int largestChannelDiff;
} clImageDiff;

typedef struct clImageSRGBHighlightPixel
{
    float x;
    float y;
    float Y;
    float nits;
    float maxNits;
    float outOfGamut;
} clImageSRGBHighlightPixel;

typedef struct clImageSRGBHighlightPixelInfo
{
    int pixelCount;
    clImageSRGBHighlightPixel * pixels;
} clImageSRGBHighlightPixelInfo;
clImageSRGBHighlightPixelInfo * clImageSRGBHighlightPixelInfoCreate(struct clContext * C, int pixelCount);
void clImageSRGBHighlightPixelInfoDestroy(struct clContext * C, clImageSRGBHighlightPixelInfo * pixelInfo);

typedef struct clImageSRGBHighlightStats
{
    int overbrightPixelCount;
    int outOfGamutPixelCount;
    int bothPixelCount; // overbright + out-of-gamut
    int hdrPixelCount;  // the sum of the above values
    int pixelCount;
    int brightestPixelX;
    int brightestPixelY;
    float brightestPixelNits;
} clImageSRGBHighlightStats;

clImage * clImageCreate(struct clContext * C, int width, int height, int depth, struct clProfile * profile);
clImage * clImageRotate(struct clContext * C, clImage * image, int cwTurns);
clImage * clImageConvert(struct clContext * C, clImage * srcImage, int taskCount, int depth, struct clProfile * dstProfile, clTonemap tonemap);
clImage * clImageCrop(struct clContext * C, clImage * srcImage, int x, int y, int w, int h, clBool keepSrc);
clImage * clImageApplyHALD(struct clContext * C, clImage * image, clImage * hald, int haldDims);
clImage * clImageResize(struct clContext * C, clImage * image, int width, int height, clFilter resizeFilter);
clImage * clImageBlend(struct clContext * C, clImage * image, clImage * compositeImage, int taskCount, clBlendParams * blendParams);
clImage * clImageCreateSRGBHighlight(clContext * C, clImage * srcImage, int srgbLuminance, clImageSRGBHighlightStats * stats, clImageSRGBHighlightPixelInfo * outPixelInfo, struct cJSON ** highlightInfoJSON);
clBool clImageAdjustRect(struct clContext * C, clImage * image, int * x, int * y, int * w, int * h);
void clImageColorGrade(struct clContext * C, clImage * image, int taskCount, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose);
void clImageSetPixel(struct clContext * C, clImage * image, int x, int y, int r, int g, int b, int a);
void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent);
void clImageDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clImage * image, int x, int y, int w, int h);
void clImageDebugDumpPixel(struct clContext * C, clImage * image, int x, int y, clImagePixelInfo * pixelInfo);
void clImageDestroy(struct clContext * C, clImage * image);
void clImageLogCreate(struct clContext * C, int width, int height, int depth, struct clProfile * profile);
clImage * clImageParseString(struct clContext * C, const char * str, int depth, struct clProfile * profile);
clBool clImageCalcSignals(struct clContext * C, int taskCount, clImage * srcImage, clImage * dstImage, clImageSignals * signals);

clImageDiff * clImageDiffCreate(struct clContext * C, clImage * image1, clImage * image2, int taskCount, float minIntensity, int threshold);
void clImageDiffUpdate(struct clContext * C, clImageDiff * diff, int threshold);
void clImageDiffDestroy(struct clContext * C, clImageDiff * diff);

void clImageToRGB8(struct clContext * C, clImage * image, uint8_t * outPixels);
void clImageFromRGB8(struct clContext * C, clImage * image, uint8_t * inPixels);
void clImageToRGBA8(struct clContext * C, clImage * image, uint8_t * outPixels);
void clImageFromRGBA8(struct clContext * C, clImage * image, uint8_t * inPixels);
void clImageToBGRA8(struct clContext * C, clImage * image, uint8_t * outPixels);
void clImageFromBGRA8(struct clContext * C, clImage * image, uint8_t * inPixels);

#endif // ifndef COLORIST_IMAGE_H
