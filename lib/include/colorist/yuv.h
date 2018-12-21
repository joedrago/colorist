// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_YUV_H
#define COLORIST_YUV_H

#include "colorist/context.h"
#include "colorist/types.h"

struct clContext;
struct clProfile;

// Using the math found here: https://www.khronos.org/registry/DataFormat/specs/1.2/dataformat.1.2.html#MODEL_YUV

typedef struct clYUV
{
    float kr;
    float kg;
    float kb;
} clYUV;

clYUV * clYUVCreate(struct clContext * C, struct clProfile * rgbProfile);
void clYUVDestroy(struct clContext * C, clYUV * yuv);

void clYUVConvertRGBtoYUV444(struct clContext * C, struct clYUV * yuv, float * rgbPixel, float * yuvPixel);
void clYUVConvertYUV444toRGB(struct clContext * C, struct clYUV * yuv, float * yuvPixel, float * rgbPixel);

#endif // ifndef COLORIST_TRANSFORM_H
