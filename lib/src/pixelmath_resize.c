#include "colorist/pixelmath.h"

#include "colorist/context.h"

#include "stb_image_resize.h"

#include <string.h>

void clPixelMathResize(struct clContext * C, int srcW, int srcH, float * srcPixels, int dstW, int dstH, float * dstPixels, clFilter filter)
{
    COLORIST_UNUSED(C);

    if (filter == CL_FILTER_NEAREST) {
        // colorist's very own super-obvious nearest neighbor implementation
        float scaleW = (float)srcW / (float)dstW;
        float scaleH = (float)srcH / (float)dstH;
        for (int j = 0; j < dstH; ++j) {
            for (int i = 0; i < dstW; ++i) {
                float * srcPixel;
                float * dstPixel;
                int srcX = (int)(((float)i + 0.5f) * scaleW);
                int srcY = (int)(((float)j + 0.5f) * scaleH);
                srcX = CL_CLAMP(srcX, 0, srcW - 1);
                srcY = CL_CLAMP(srcY, 0, srcH - 1);
                srcPixel = &srcPixels[4 * (srcX + (srcY * srcW))];
                dstPixel = &dstPixels[4 * (i + (j * dstW))];
                memcpy(dstPixel, srcPixel, 4 * sizeof(float));
            }
        }
    } else {
        // use STB!
        stbir_resize_float_generic(srcPixels,
                                   srcW,
                                   srcH,
                                   srcW * 4 * sizeof(float),
                                   dstPixels,
                                   dstW,
                                   dstH,
                                   dstW * 4 * sizeof(float),
                                   4,
                                   3,
                                   0,
                                   STBIR_EDGE_CLAMP,
                                   (stbir_filter)filter,
                                   STBIR_COLORSPACE_LINEAR,
                                   NULL);
    }
}
