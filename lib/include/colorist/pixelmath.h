// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_PIXELMATH_H
#define COLORIST_PIXELMATH_H

#include "colorist/context.h"
#include "colorist/types.h"

struct clContext;
struct clProfile;

float clPixelMathRoundf(float val);
float clPixelMathFloorf(float val);
clBool clPixelMathEqualsf(float a, float b);
float clPixelMathRoundNormalized(float normalizedValue, float factor); // Clamps normalizedValue int [0,1], then scales by factor, then rounds. Used in unorm conversion
void clPixelMathUNormToFloat(struct clContext * C, uint16_t * inPixels, int inDepth, float * outPixels, int pixelCount);
void clPixelMathFloatToUNorm(struct clContext * C, float * inPixels, uint16_t * outPixels, int outDepth, int pixelCount);
void clPixelMathScaleLuminance(struct clContext * C, float * pixels, int pixelCount, float luminanceScale, clBool tonemap);
void clPixelMathColorGrade(struct clContext * C,
                           int taskCount,
                           struct clProfile * pixelProfile,
                           float * pixels,
                           int pixelCount,
                           int imageWidth,
                           int srcLuminance,
                           int dstColorDepth,
                           int * outLuminance,
                           float * outGamma,
                           clBool verbose);
void clPixelMathResize(struct clContext * C, int srcW, int srcH, float * srcPixels, int dstW, int dstH, float * dstPixels, clFilter filter);
void clPixelMathHaldCLUTLookup(struct clContext * C, float * haldData, int haldDims, const float src[4], float dst[4]);

#endif
