#include "colorist/pixelmath.h"

#include "colorist/context.h"

#include <string.h>

#if 0
static float bilinear(const float interpX, const float interpY, float c00, float c10, float c01, float c11)
{
    float a = c00 * (1 - interpX) + c10 * interpX;
    float b = c01 * (1 - interpX) + c11 * interpX;
    return a * (1 - interpY) + b * interpY;
}
#endif

void clPixelMathResize(struct clContext * C, int srcW, int srcH, float * srcPixels, int dstW, int dstH, float * dstPixels, clFilter filter)
{
    if (1) { //(filter == CL_FILTER_NEAREST) {
        int i, j;
        for (j = 0; j < dstH; ++j) {
            for (i = 0; i < dstW; ++i) {
                float * srcPixel;
                float * dstPixel;

                int srcX = (int)((float)i / (float)(dstW - 1) * (srcW - 1));
                int srcY = (int)((float)j / (float)(dstH - 1) * (srcH - 1));

                srcPixel = &srcPixels[4 * (srcX + (srcY * srcW))];
                dstPixel = &dstPixels[4 * (i + (j * dstW))];
                memcpy(dstPixel, srcPixel, 4 * sizeof(float));
            }
        }
    } else {

        // TODO: This code is bad / broken. completely replace
#if 0
        // Bilinear

        int srcLastX =  srcW - 1;
        int srcLastY =  srcH - 1;
        int dstLastX =  dstW - 1;
        int dstLastY =  dstH - 1;
        float stepX = (float)srcW / (float)dstW;
        float stepY = (float)srcH / (float)dstH;

        int i, j;
        for (j = 0; j < dstH; ++j) {
            for (i = 0; i < dstW; ++i) {
                float * srcPixel00, * srcPixel01, * srcPixel10, * srcPixel11;
                float * dstPixel;
                float interpX, interpY;
                int srcX0, srcY0, srcX1, srcY1;

                interpX = (((float)i / (float)dstLastX) * srcLastX); // - 0.5f;
                interpY = (((float)j / (float)dstLastY) * srcLastY); // - 0.5f;
                interpX = CL_CLAMP(interpX, 0, srcLastX);
                interpY = CL_CLAMP(interpY, 0, srcLastY);
                srcX0 = (int)interpX;
                srcY0 = (int)interpY;
                interpX -= srcX0;
                interpY -= srcY0;

                srcX1 = (int)((float)srcX0 + stepX);
                srcX1 = CL_CLAMP(srcX1, 0, srcLastX);
                srcY1 = (int)((float)srcY0 + stepY);
                srcY1 = CL_CLAMP(srcY1, 0, srcLastY);

                srcPixel00 = &srcPixels[4 * (srcX0 + (srcY0 * srcW))];
                srcPixel10 = &srcPixels[4 * (srcX1 + (srcY0 * srcW))];
                srcPixel01 = &srcPixels[4 * (srcX0 + (srcY1 * srcW))];
                srcPixel11 = &srcPixels[4 * (srcX1 + (srcY1 * srcW))];

                dstPixel = &dstPixels[4 * (i + (j * dstW))];
                dstPixel[0] = bilinear(interpX, interpY, srcPixel00[0], srcPixel10[0], srcPixel01[0], srcPixel11[0]);
                dstPixel[1] = bilinear(interpX, interpY, srcPixel00[1], srcPixel10[1], srcPixel01[1], srcPixel11[1]);
                dstPixel[2] = bilinear(interpX, interpY, srcPixel00[2], srcPixel10[2], srcPixel01[2], srcPixel11[2]);
                dstPixel[3] = bilinear(interpX, interpY, srcPixel00[3], srcPixel10[3], srcPixel01[3], srcPixel11[3]);
            }
        }
#endif  /* if 0 */
    }
}
