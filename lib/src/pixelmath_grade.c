#include "colorist/pixelmath.h"

#include <math.h>

// NOTE: This is a work in progress. There are probably lots of problems with this.

clBool clPixelMathColorGrade(float * pixels, int pixelCount, int srcLuminance, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose)
{
    int maxLuminance = 0;
    float bestGamma = 0.0f;
    float * pixel;
    int i;

    // Find max luminance
    if (*outLuminance == 0) {
        // TODO: This should probably be some kind of histogram which spends most of the codepoints where most of the pixel values are.

        float maxChannel = 0.0f;

        pixel = pixels;
        for (i = 0; i < pixelCount; ++i) {
            maxChannel = (maxChannel > pixel[0]) ? maxChannel : pixel[0];
            maxChannel = (maxChannel > pixel[1]) ? maxChannel : pixel[1];
            maxChannel = (maxChannel > pixel[2]) ? maxChannel : pixel[2];
            pixel += 4;
        }

        maxLuminance = (int)(maxChannel * srcLuminance);
        maxLuminance = CL_CLAMP(maxLuminance, 0, srcLuminance);
        printf(" * [grading] Found max luminance: %d nits\n", maxLuminance);
    } else {
        maxLuminance = *outLuminance;
        printf(" * [grading] Using requested max luminance: %d nits\n", maxLuminance);
    }

    // Find best gamma
    if (*outGamma == 0.0f) {
        float luminanceScale = (float)srcLuminance / maxLuminance;
        int gammaInt;
        int minGammaInt;
        float minErrorTerm = -1.0f;
        float maxChannel = (dstColorDepth == 16) ? 65535.0f : 255.0f;
        for (gammaInt = 20; gammaInt <= 50; ++gammaInt) { // (2.0 - 5.0) by 0.1
            float gammaAttempt = (float)gammaInt / 10.0f;
            float invGamma = 1.0f / gammaAttempt;
            float errorTerm = 0.0f;
            float scaledChannel;
            pixel = pixels;
            for (i = 0; i < pixelCount; ++i) {
                scaledChannel = pixel[0] * luminanceScale;
                scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
                errorTerm += fabsf(scaledChannel - powf(roundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gammaAttempt));

                scaledChannel = pixel[1] * luminanceScale;
                scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
                errorTerm += fabsf(scaledChannel - powf(roundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gammaAttempt));

                scaledChannel = pixel[2] * luminanceScale;
                scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
                errorTerm += fabsf(scaledChannel - powf(roundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gammaAttempt));

                pixel += 4;
            }
            if (minErrorTerm < 0.0f) {
                minErrorTerm = errorTerm;
                minGammaInt = gammaInt;
            } else if (minErrorTerm > errorTerm) {
                minErrorTerm = errorTerm;
                minGammaInt = gammaInt;
            }
            if (verbose)
                printf(" * [grading] gamma attempt (%g) error term: %g (best gamma is %g at error term %g)\n", gammaAttempt, errorTerm, (float)minGammaInt / 10.0f, minErrorTerm);
        }
        bestGamma = (float)minGammaInt / 10.0f;
        printf(" * [grading] Found best gamma: %g\n", bestGamma);
    } else {
        bestGamma = *outGamma;
        printf(" * [grading] Using requested gamma: %g\n", bestGamma);
    }

    *outLuminance = maxLuminance;
    *outGamma = bestGamma;
    return clTrue;
}
