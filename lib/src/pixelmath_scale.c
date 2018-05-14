#include "colorist/pixelmath.h"

#include "colorist/context.h"

void clPixelMathUNormToFloat(struct clContext * C, uint8_t * inPixels, int inDepth, float * outPixels, int pixelCount)
{
    float maxChannel = (float)((1 << inDepth) - 1);
    int channelCount = pixelCount * 4;
    int i;

    if (inDepth > 8) {
        uint16_t * inChannel = (uint16_t *)inPixels;
        float * outChannel = outPixels;
        for (i = 0; i < channelCount; ++i) {
            outChannel[i] = inChannel[i] / maxChannel;
        }
    } else {
        uint8_t * inChannel = inPixels;
        float * outChannel = outPixels;
        for (i = 0; i < channelCount; ++i) {
            outChannel[i] = inChannel[i] / maxChannel;
        }
    }
}

void clPixelMathFloatToUNorm(struct clContext * C, float * inPixels, uint8_t * outPixels, int outDepth, int pixelCount)
{
    float maxChannel = (float)((1 << outDepth) - 1);
    int channelCount = pixelCount * 4;
    int i;

    if (outDepth > 8) {
        float * inChannel = inPixels;
        uint16_t * outChannel = (uint16_t *)outPixels;
        for (i = 0; i < channelCount; ++i) {
            outChannel[i] = (uint16_t)clPixelMathRoundf(inChannel[i] * maxChannel);
        }
    } else {
        float * inChannel = inPixels;
        uint8_t * outChannel = outPixels;
        for (i = 0; i < channelCount; ++i) {
            outChannel[i] = (uint8_t)clPixelMathRoundf(inChannel[i] * maxChannel);
        }
    }
}

void clPixelMathScaleLuminance(struct clContext * C, float * pixels, int pixelCount, float luminanceScale, clBool tonemap)
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
