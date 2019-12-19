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
static const uint32_t CL_BYTES_PER_CHANNEL[CL_PIXELFORMAT_COUNT] = { (uint32_t)sizeof(uint8_t),
                                                                     (uint32_t)sizeof(uint16_t),
                                                                     (uint32_t)sizeof(float) };
#define CL_BYTES_PER_PIXEL(PIXELFORMAT) (CL_CHANNELS_PER_PIXEL * CL_BYTES_PER_CHANNEL[PIXELFORMAT])

struct clProfile;
struct clRaw;
struct cJSON;

typedef struct clImage
{
    int width;
    int height;
    int depth;
    struct clProfile * profile;

    // By default, all of these pixels ptrs are just NULL. To use them,
    // you must prepare them using either clImagePrepareReadPixels()
    // or clImagePrepareWritePixels().
    uint8_t * pixelsU8;
    uint16_t * pixelsU16;
    float * pixelsF32;
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

typedef struct clImageHDRPixel
{
    float x;
    float y;
    float Y;
    float nits;
    float maxNits;
    float saturation; // 0-1 is BT709 saturation, 1-2 is "out of gamut" saturation (normalized to that edge)
} clImageHDRPixel;

typedef struct clImageHDRPixelInfo
{
    int pixelCount;
    clImageHDRPixel * pixels;
} clImageHDRPixelInfo;
clImageHDRPixelInfo * clImageHDRPixelInfoCreate(struct clContext * C, int pixelCount);
void clImageHDRPixelInfoDestroy(struct clContext * C, clImageHDRPixelInfo * pixelInfo);

typedef struct clImageHDRStats
{
    int overbrightPixelCount;
    int outOfGamutPixelCount;
    int bothPixelCount; // overbright + out-of-gamut
    int hdrPixelCount;  // the sum of the above values
    int pixelCount;
    int brightestPixelX;
    int brightestPixelY;
    float brightestPixelNits;
} clImageHDRStats;

typedef struct clImageHDRPercentile
{
    float nits;       // [0-maxNits], HDR10 maxNits is 10000
    float saturation; // [0-2]. 0-1 is "in-srgb-gamut saturation", 1-2 is "how far out of gamut is it"
} clImageHDRPercentile;

#define CL_QUANTIZATION_BUCKET_COUNT 1024
typedef struct clImageHDRQuantization
{
    clImageHDRPercentile percentiles[101];
    int pixelCountsNitsPQ[CL_QUANTIZATION_BUCKET_COUNT];     // pixels counts quantized into nits values on the PQ curve
    int pixelCountsSaturation[CL_QUANTIZATION_BUCKET_COUNT]; // pixel counts quantized from (0-1 / 1023), see saturation comment above
} clImageHDRQuantization;

clImage * clImageCreate(struct clContext * C, int width, int height, int depth, struct clProfile * profile);
clImage * clImageRotate(struct clContext * C, clImage * image, int cwTurns);
clImage * clImageConvert(struct clContext * C,
                         clImage * srcImage,
                         int depth,
                         struct clProfile * dstProfile,
                         clTonemap tonemap,
                         clTonemapParams * tonemapParams);
clImage * clImageCrop(struct clContext * C, clImage * srcImage, int x, int y, int w, int h, clBool keepSrc);
clImage * clImageApplyHALD(struct clContext * C, clImage * image, clImage * hald, int haldDims);
clImage * clImageResize(struct clContext * C, clImage * image, int width, int height, clFilter resizeFilter);
clImage * clImageBlend(struct clContext * C, clImage * image, clImage * compositeImage, clBlendParams * blendParams);
void clImageMeasureHDR(clContext * C,
                       clImage * srcImage,
                       int srgbLuminance,
                       float satLuminance,
                       clImage ** outImage,
                       clImageHDRStats * outStats,
                       clImageHDRPixelInfo * outPixelInfo,
                       clImageHDRQuantization * outQuantization);
void clImagePrepareReadPixels(struct clContext * C, clImage * image, clPixelFormat pixelFormat);
void clImagePrepareWritePixels(struct clContext * C, clImage * image, clPixelFormat pixelFormat);
clBool clImageAdjustRect(struct clContext * C, clImage * image, int * x, int * y, int * w, int * h);
void clImageColorGrade(struct clContext * C, clImage * image, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose);
void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent);
void clImageDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clImage * image, int x, int y, int w, int h);
void clImageDebugDumpPixel(struct clContext * C, clImage * image, int x, int y, clImagePixelInfo * pixelInfo);
void clImageDestroy(struct clContext * C, clImage * image);
void clImageLogCreate(struct clContext * C, int width, int height, int depth, struct clProfile * profile);
clImage * clImageParseString(struct clContext * C, const char * str, int depth, struct clProfile * profile);
clBool clImageCalcSignals(struct clContext * C, clImage * srcImage, clImage * dstImage, clImageSignals * signals);
float clImageLargestChannel(struct clContext * C, clImage * image);
float clImagePeakLuminance(struct clContext * C, clImage * image); // Doesn't return maxCLL, but the lum of (largestChannel, largestChannel, largestChannel)
void clImageClear(struct clContext * C, clImage * image, float color[4]);
void clImageDrawCIE(struct clContext * C, clImage * image, float borderColor[4], int borderThickness);
void clImageDrawGamut(struct clContext * C,
                      clImage * image,
                      struct clProfilePrimaries * primaries,
                      float color[4],
                      int thickness,
                      float wpColor[4],
                      int wpThickness);
void clImageDrawLine(struct clContext * C, clImage * image, int x0, int y0, int x1, int y1, float color[4], int thickness);

clImageDiff * clImageDiffCreate(struct clContext * C, clImage * image1, clImage * image2, float minIntensity, int threshold);
void clImageDiffUpdate(struct clContext * C, clImageDiff * diff, int threshold);
void clImageDiffDestroy(struct clContext * C, clImageDiff * diff);

#endif // ifndef COLORIST_IMAGE_H
