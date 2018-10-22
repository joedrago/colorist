// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_TRANSFORM_H
#define COLORIST_TRANSFORM_H

#include "colorist/types.h"

// for cmsHPROFILE, cmsHTRANSFORM
#include "lcms2.h"

struct clContext;
struct clProfile;

typedef enum clTransformFormat
{
    CL_TF_XYZ_FLOAT = 0,
    CL_TF_RGB_FLOAT,
    CL_TF_RGBA_FLOAT,
    CL_TF_RGBA_8,
    CL_TF_RGBA_16
} clTransformFormat;

// clTransform does not own either clProfile and it is expected that both will outlive the clTransform that uses them
typedef struct clTransform
{
    struct clProfile * srcProfile; // If NULL, is XYZ profile
    struct clProfile * dstProfile; // If NULL, is XYZ profile
    clTransformFormat srcFormat;
    clTransformFormat dstFormat;

    // Cache for LittleCMS objects
    cmsHPROFILE xyzProfile;
    cmsHTRANSFORM hTransform;
} clTransform;

clTransform * clTransformCreate(struct clContext * C, struct clProfile * srcProfile, clTransformFormat srcFormat, struct clProfile * dstProfile, clTransformFormat dstFormat);
void clTransformDestroy(struct clContext * C, clTransform * transform);

void clTransformRun(struct clContext * C, clTransform * transform, int taskCount, void * srcPixels, void * dstPixels, int pixelCount);

#endif // ifndef COLORIST_TRANSFORM_H
