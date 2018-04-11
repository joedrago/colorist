#include "colorist/pixelmath.h"

clBool clPixelMathColorGrade(float * pixels, int pixelCount, int srcLuminance, int dstColorDepth, int * outLuminance, float * outGamma)
{
    // TODO: Implement

    *outLuminance = 225;
    *outGamma = 2.4f;
    return clTrue;
}
