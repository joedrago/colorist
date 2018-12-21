// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/yuv.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

clYUV * clYUVCreate(struct clContext * C, struct clProfile * rgbProfile)
{
    clYUV * yuv = clAllocateStruct(clYUV);

    clProfilePrimaries primaries;
    clProfileQuery(C, rgbProfile, &primaries, NULL, NULL);

    clProfileCurve gamma1;
    gamma1.type = CL_PCT_GAMMA;
    gamma1.gamma = 1.0f;
    clProfile * linearProfile = clProfileCreate(C, &primaries, &gamma1, 1, NULL);
    clTransform * linearToXYZ = clTransformCreate(C, linearProfile, CL_XF_RGBA, 32, NULL, CL_XF_XYZ, 32, CL_TONEMAP_OFF);
    clTransform * linearFromXYZ = clTransformCreate(C, NULL, CL_XF_XYZ, 32, linearProfile, CL_XF_RGB, 32, CL_TONEMAP_OFF);

    yuv->kr = clTransformCalcMaxY(C, linearFromXYZ, linearToXYZ, primaries.red[0], primaries.red[1]);
    yuv->kg = clTransformCalcMaxY(C, linearFromXYZ, linearToXYZ, primaries.green[0], primaries.green[1]);
    yuv->kb = clTransformCalcMaxY(C, linearFromXYZ, linearToXYZ, primaries.blue[0], primaries.blue[1]);

    clTransformDestroy(C, linearToXYZ);
    clTransformDestroy(C, linearFromXYZ);
    clProfileDestroy(C, linearProfile);
    return yuv;
}

void clYUVDestroy(struct clContext * C, clYUV * yuv)
{
    clFree(yuv);
}

void clYUVConvertRGBtoYUV444(struct clContext * C, struct clYUV * yuv, float * rgbPixel, float * yuvPixel)
{
    COLORIST_UNUSED(C);

    float Y = (yuv->kr * rgbPixel[0]) + (yuv->kg * rgbPixel[1]) + (yuv->kb * rgbPixel[2]);
    yuvPixel[0] = Y;
    yuvPixel[1] = (rgbPixel[2] - Y) / (2 * (1 - yuv->kb));
    yuvPixel[2] = (rgbPixel[0] - Y) / (2 * (1 - yuv->kr));
}

void clYUVConvertYUV444toRGB(struct clContext * C, struct clYUV * yuv, float * yuvPixel, float * rgbPixel)
{
    COLORIST_UNUSED(C);

    float Y  = yuvPixel[0];
    float Cb = yuvPixel[1];
    float Cr = yuvPixel[2];

    float R = Y + (2 * (1 - yuv->kr)) * Cr;
    float B = Y + (2 * (1 - yuv->kb)) * Cb;
    float G = Y - (
        (2 * ((yuv->kr * (1 - yuv->kr) * Cr) + (yuv->kb * (1 - yuv->kb) * Cb)))
        /
        yuv->kg);

    rgbPixel[0] = CL_CLAMP(R, 0.0f, 1.0f);
    rgbPixel[1] = CL_CLAMP(G, 0.0f, 1.0f);
    rgbPixel[2] = CL_CLAMP(B, 0.0f, 1.0f);
}
