// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_PIXELMATH_H
#define COLORIST_PIXELMATH_H

#include "colorist/types.h"

struct clContext;

float clPixelMathRoundf(float val);
void clPixelMathUNormToFloat(struct clContext * C, uint8_t * inPixels, int inDepth, float * outPixels, int pixelCount);
void clPixelMathFloatToUNorm(struct clContext * C, float * inPixels, uint8_t * outPixels, int outDepth, int pixelCount);
void clPixelMathScaleLuminance(struct clContext * C, float * pixels, int pixelCount, float luminanceScale, clBool tonemap);
void clPixelMathColorGrade(struct clContext * C, int taskCount, float * pixels, int pixelCount, int srcLuminance, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose);

#endif
