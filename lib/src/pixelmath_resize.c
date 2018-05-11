#include "colorist/pixelmath.h"

#include "colorist/context.h"

#include "stb_image_resize.h"

#include <string.h>

void clPixelMathResize(struct clContext * C, int srcW, int srcH, float * srcPixels, int dstW, int dstH, float * dstPixels, clFilter filter)
{
    if (filter == CL_FILTER_NEAREST) {
#if 0
        float scaleW = (float)(srcW - 1) / (float)(dstW - 1);
        float scaleH = (float)(srcH - 1) / (float)(dstH - 1);
        int i, j;

        for (j = 0; j < dstH; ++j) {
            for (i = 0; i < dstW; ++i) {
                float * srcPixel;
                float * dstPixel;

                int srcX = (int)clPixelMathRoundf(((float)i * scaleW) + 0.5f);
                int srcY = (int)clPixelMathRoundf(((float)j * scaleH) + 0.5f);
                srcX = CL_CLAMP(srcX, 0, srcW);
                srcY = CL_CLAMP(srcY, 0, srcH);

                srcPixel = &srcPixels[4 * (srcX + (srcY * srcW))];
                dstPixel = &dstPixels[4 * (i + (j * dstW))];
                memcpy(dstPixel, srcPixel, 4 * sizeof(float));
            }
        }
#endif

        // float scaleW = (float)srcW / (float)dstW;
        // float scaleH = (float)srcH / (float)dstH;
        int i, j;

        for (j = 0; j < dstH; ++j) {
            for (i = 0; i < dstW; ++i) {
                float * srcPixel;
                float * dstPixel;

                int srcX = (int)((float)i * (float)srcW / (float)dstW);
                int srcY = (int)((float)j * (float)srcW / (float)dstW);
                srcX = CL_CLAMP(srcX, 0, srcW - 1);
                srcY = CL_CLAMP(srcY, 0, srcH - 1);

                srcPixel = &srcPixels[4 * (srcX + (srcY * srcW))];
                dstPixel = &dstPixels[4 * (i + (j * dstW))];
                memcpy(dstPixel, srcPixel, 4 * sizeof(float));
                dstPixel[0] = 1.0f;
            }
        }

    } else {
        // use STB!

        stbir_resize_float_generic(
            srcPixels, srcW, srcH, srcW * 4 * sizeof(float),
            dstPixels, dstW, dstH, dstW * 4 * sizeof(float),
            4, 3, 0,
            STBIR_EDGE_CLAMP, filter, STBIR_COLORSPACE_LINEAR,
            NULL);
    }
}
