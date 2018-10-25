// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_CCMM_H
#define COLORIST_CCMM_H

#include "colorist/types.h"

struct clContext;
struct clTransform;

// This is internally used by clTransform; use that API
void clCCMMPrepareTransform(struct clContext * C, struct clTransform * transform);
void clCCMMTransform(struct clContext * C, struct clTransform * transform, void * srcPixels, void * dstPixels, int pixelCount);

// Uncomment to debug CCMM matrix math via colorist-test
// #define DEBUG_MATRIX_MATH 1

#endif
