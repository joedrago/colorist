// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include <string.h>

clBool clImageCalcSignals(struct clContext * C, clImage * srcImage, clImage * dstImage, clImageSignals * signals)
{
    memset(signals, 0, sizeof(*signals));

    if ((srcImage->width != dstImage->width) || (srcImage->height != dstImage->height)) {
        clContextLogError(C, "Conversion stats unavailable on images of different sizes");
        return clFalse;
    }

    int pixelCount = srcImage->width * srcImage->height;

    int srcLuminance, dstLuminance;
    clProfileQuery(C, srcImage->profile, NULL, NULL, &srcLuminance);
    clProfileQuery(C, dstImage->profile, NULL, NULL, &dstLuminance);
    int maxLuminance = srcLuminance;
    if (maxLuminance < dstLuminance) {
        maxLuminance = dstLuminance;
    }
    float maxLuminanceF = (float)maxLuminance;

    clImagePrepareReadPixels(C, srcImage, CL_PIXELFORMAT_F32);
    clTransform * srcToXYZ = clTransformCreate(C, srcImage->profile, CL_XF_RGBA, NULL, CL_XF_XYZ, CL_TONEMAP_OFF);
    float * srcXYZ = clAllocate(3 * sizeof(float) * pixelCount);
    clTransformRun(C, srcToXYZ, srcImage->pixelsF32, srcXYZ, pixelCount);
    clTransformDestroy(C, srcToXYZ);

    clImagePrepareReadPixels(C, dstImage, CL_PIXELFORMAT_F32);
    clTransform * dstToXYZ = clTransformCreate(C, dstImage->profile, CL_XF_RGBA, NULL, CL_XF_XYZ, CL_TONEMAP_OFF);
    float * dstXYZ = clAllocate(3 * sizeof(float) * pixelCount);
    clTransformRun(C, dstToXYZ, dstImage->pixelsF32, dstXYZ, pixelCount);
    clTransformDestroy(C, dstToXYZ);

    float errorSquaredSumLinear = 0.0f;
    float errorSquaredSumG22 = 0.0f;
    float gamma = 1.0f / 2.2f;
    for (int i = 0; i < pixelCount; ++i) {
        float * srcPixel = &srcXYZ[3 * i];
        float * dstPixel = &dstXYZ[3 * i];

        float normLinearSrcXYZ[3];
        normLinearSrcXYZ[0] = srcPixel[0] / maxLuminanceF;
        normLinearSrcXYZ[0] = CL_CLAMP(normLinearSrcXYZ[0], 0.0f, 1.0f);
        normLinearSrcXYZ[1] = srcPixel[1] / maxLuminanceF;
        normLinearSrcXYZ[1] = CL_CLAMP(normLinearSrcXYZ[1], 0.0f, 1.0f);
        normLinearSrcXYZ[2] = srcPixel[2] / maxLuminanceF;
        normLinearSrcXYZ[2] = CL_CLAMP(normLinearSrcXYZ[2], 0.0f, 1.0f);

        float normLinearDstXYZ[3];
        normLinearDstXYZ[0] = dstPixel[0] / maxLuminanceF;
        normLinearDstXYZ[0] = CL_CLAMP(normLinearDstXYZ[0], 0.0f, 1.0f);
        normLinearDstXYZ[1] = dstPixel[1] / maxLuminanceF;
        normLinearDstXYZ[1] = CL_CLAMP(normLinearDstXYZ[1], 0.0f, 1.0f);
        normLinearDstXYZ[2] = dstPixel[2] / maxLuminanceF;
        normLinearDstXYZ[2] = CL_CLAMP(normLinearDstXYZ[2], 0.0f, 1.0f);

        float normLinearDiff[3];
        normLinearDiff[0] = normLinearDstXYZ[0] - normLinearSrcXYZ[0];
        normLinearDiff[1] = normLinearDstXYZ[1] - normLinearSrcXYZ[1];
        normLinearDiff[2] = normLinearDstXYZ[2] - normLinearSrcXYZ[2];
        errorSquaredSumLinear += (normLinearDiff[0] * normLinearDiff[0]) + (normLinearDiff[1] * normLinearDiff[1]) +
                                 (normLinearDiff[2] * normLinearDiff[2]);

        float normG22SrcXYZ[3];
        normG22SrcXYZ[0] = powf(normLinearSrcXYZ[0], gamma);
        normG22SrcXYZ[1] = powf(normLinearSrcXYZ[1], gamma);
        normG22SrcXYZ[2] = powf(normLinearSrcXYZ[2], gamma);

        float normG22DstXYZ[3];
        normG22DstXYZ[0] = powf(normLinearDstXYZ[0], gamma);
        normG22DstXYZ[1] = powf(normLinearDstXYZ[1], gamma);
        normG22DstXYZ[2] = powf(normLinearDstXYZ[2], gamma);

        float normG22Diff[3];
        normG22Diff[0] = normG22DstXYZ[0] - normG22SrcXYZ[0];
        normG22Diff[1] = normG22DstXYZ[1] - normG22SrcXYZ[1];
        normG22Diff[2] = normG22DstXYZ[2] - normG22SrcXYZ[2];
        errorSquaredSumG22 += (normG22Diff[0] * normG22Diff[0]) + (normG22Diff[1] * normG22Diff[1]) + (normG22Diff[2] * normG22Diff[2]);
    }

    if (errorSquaredSumLinear > 0.0f) {
        signals->mseLinear = errorSquaredSumLinear / (float)pixelCount;
        signals->psnrLinear = 10.0f * log10f(1.0f / signals->mseLinear);
    } else {
        signals->psnrLinear = INFINITY;
    }
    if (errorSquaredSumG22 > 0.0f) {
        signals->mseG22 = errorSquaredSumG22 / (float)pixelCount;
        signals->psnrG22 = 10.0f * log10f(1.0f / signals->mseG22);
    } else {
        signals->psnrG22 = INFINITY;
    }

    clFree(srcXYZ);
    clFree(dstXYZ);
    return clTrue;
}
