#include "colorist/pixelmath.h"

#include "colorist/context.h"

// If you're closer than this to a color, just use that color exclusively
#define MIN_DISTANCE (0.000001f)

// Converts ([0-1], [0-1], [0-1]) to a hald data or weight index in [0-7]
#define I(X, Y, Z) ((X) + ((Y)*2) + ((Z)*4))

void clPixelMathHaldCLUTLookup(struct clContext * C, float * haldData, int haldDims, const float src[4], float dst[4])
{
    COLORIST_UNUSED(C);

    float idealR = src[0] * (haldDims - 1);
    float idealG = src[1] * (haldDims - 1);
    float idealB = src[2] * (haldDims - 1);

    float firstCornerX = clPixelMathFloorf(idealR);
    float firstCornerY = clPixelMathFloorf(idealG);
    float firstCornerZ = clPixelMathFloorf(idealB);
    firstCornerX = CL_CLAMP(firstCornerX, 0, haldDims - 2);
    firstCornerY = CL_CLAMP(firstCornerY, 0, haldDims - 2);
    firstCornerZ = CL_CLAMP(firstCornerZ, 0, haldDims - 2);

    float distSquared[8];
    int dist0Index = -1; // Index containing a distance of 0, if any
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                float dx = (firstCornerX + x) - idealR;
                float dy = (firstCornerY + y) - idealG;
                float dz = (firstCornerZ + z) - idealB;
                float dsq = (dx * dx) + (dy * dy) + (dz * dz);

                distSquared[I(x, y, z)] = dsq;
                if (dsq < MIN_DISTANCE) {
                    dist0Index = I(x, y, z);
                }
            }
        }
    }

    float weights[8];
    float idwDivisor;
    if (dist0Index == -1) {
        // No color with a distance of 0 found, use IDW
        idwDivisor = 0.0f;
        for (int i = 0; i < 8; ++i) {
            weights[i] = 1.0f / distSquared[i];
            idwDivisor += weights[i];
        }
    } else {
        // Give all of the weight to one color, skip IDW
        for (int i = 0; i < 8; ++i) {
            weights[i] = 0.0f;
        }
        weights[dist0Index] = 1.0f;
        idwDivisor = 1.0f; // Unused
    }

    dst[0] = 0.0f;
    dst[1] = 0.0f;
    dst[2] = 0.0f;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                int index = ((int)firstCornerX + x) + (((int)firstCornerY + y) * haldDims) +
                            (((int)firstCornerZ + z) * haldDims * haldDims);
                float * lookup = &haldData[index * 4];
                float weight = weights[I(x, y, z)];
                dst[0] += lookup[0] * weight;
                dst[1] += lookup[1] * weight;
                dst[2] += lookup[2] * weight;
            }
        }
    }
    dst[0] /= idwDivisor;
    dst[1] /= idwDivisor;
    dst[2] /= idwDivisor;

    // Copy alpha directly
    dst[3] = src[3];
}
