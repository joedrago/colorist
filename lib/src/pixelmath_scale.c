#include "colorist/pixelmath.h"

void clPixelMathScaleLuminance(float * pixels, int pixelCount, float luminanceScale, clBool tonemap)
{
    float * pixel;
    int i;

    if (tonemap) {
        pixel = pixels;
        for (i = 0; i < pixelCount; ++i) {
            pixel[0] *= luminanceScale;                     // scale
            pixel[0] = (pixel[0] < 0.0f) ? 0.0f : pixel[0]; // max(0, v)
            pixel[0] = pixel[0] / (1.0f + pixel[0]);        // reinhard tonemap
            pixel[0] = CL_CLAMP(pixel[0], 0.0f, 1.0f);      // clamp
            pixel[1] *= luminanceScale;                     // scale
            pixel[1] = (pixel[1] < 0.0f) ? 0.0f : pixel[1]; // max(0, v)
            pixel[1] = pixel[1] / (1.0f + pixel[1]);        // reinhard tonemap
            pixel[1] = CL_CLAMP(pixel[1], 0.0f, 1.0f);      // clamp
            pixel[2] *= luminanceScale;                     // scale
            pixel[2] = (pixel[2] < 0.0f) ? 0.0f : pixel[2]; // max(0, v)
            pixel[2] = pixel[2] / (1.0f + pixel[2]);        // reinhard tonemap
            pixel[2] = CL_CLAMP(pixel[2], 0.0f, 1.0f);      // clamp
            pixel += 4;
        }
    } else {
        pixel = pixels;
        for (i = 0; i < pixelCount; ++i) {
            pixel[0] *= luminanceScale;                // scale
            pixel[0] = CL_CLAMP(pixel[0], 0.0f, 1.0f); // clamp
            pixel[1] *= luminanceScale;                // scale
            pixel[1] = CL_CLAMP(pixel[1], 0.0f, 1.0f); // clamp
            pixel[2] *= luminanceScale;                // scale
            pixel[2] = CL_CLAMP(pixel[2], 0.0f, 1.0f); // clamp
            pixel += 4;
        }
    }
}
