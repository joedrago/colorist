// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_PIXELMATH_H
#define COLORIST_PIXELMATH_H

#include "colorist/types.h"

// Yes, clamp macros are nasty. Do not use them.
#define CL_CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

void clPixelMathScaleLuminance(float * pixels, int pixelCount, float luminanceScale, clBool tonemap);
clBool clPixelMathColorGrade(float * pixels, int pixelCount, int srcLuminance, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose);

#endif
