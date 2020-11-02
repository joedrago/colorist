// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_TRANSFORM_H
#define COLORIST_TRANSFORM_H

#include "colorist/context.h"
#include "colorist/types.h"

// for gbMat3
#include "gb_math.h"

// for cmsHPROFILE, cmsHTRANSFORM
#include "lcms2.h"

struct clContext;
struct clProfile;
struct clProfilePrimaries;

// Why the X's in these enums? Transform, XForm, get it? (I needed to disambiguate)

typedef enum clTransformFormat
{
    CL_XF_XYZ = 0, // 3 component, 32bit float
    CL_XF_RGB,     // 3 component, 32bit == float, 8bit == uint8_t, 9-16bit == uint16_t
    CL_XF_RGBA     // 4 component, 32bit == float, 8bit == uint8_t, 9-16bit == uint16_t
} clTransformFormat;

typedef enum clTransformTransferFunction
{
    CL_XTF_NONE = 0,
    CL_XTF_GAMMA,
    CL_XTF_SRGB,
    CL_XTF_HLG,
    CL_XTF_PQ
} clTransformTransferFunction;

// clTransform does not own either clProfile and it is expected that both will outlive the clTransform that uses them
typedef struct clTransform
{
    struct clProfile * srcProfile; // If NULL, is XYZ profile
    struct clProfile * dstProfile; // If NULL, is XYZ profile
    clTransformFormat srcFormat;
    clTransformFormat dstFormat;
    float whitePointX;
    float whitePointY;
    float srcCurveScale;
    float dstCurveScale;
    float srcLuminanceScale;
    float dstLuminanceScale;
    clTonemap requestedTonemap;
    clTonemapParams tonemapParams;
    clBool tonemapEnabled;        // calculated from incoming tonemap value
    clBool luminanceScaleEnabled; // optimization; if false, avoid all luminance scaling math

    // Cache for CCMM objects
    clTransformTransferFunction ccmmSrcEOTF;
    clTransformTransferFunction ccmmDstOETF;
    float ccmmSrcGamma;
    float ccmmDstInvGamma;
    gbMat3 ccmmSrcToXYZ;
    gbMat3 ccmmXYZToDst;
    gbMat3 ccmmCombined;
    float ccmmHLGLuminance;
    clBool ccmmReady;

    // Cache for LittleCMS objects
    cmsHPROFILE lcmsXYZProfile;
    cmsHTRANSFORM lcmsSrcToXYZ;
    cmsHTRANSFORM lcmsXYZToDst;
    cmsHTRANSFORM lcmsCombined;
    clBool lcmsReady;
} clTransform;

clTransform * clTransformCreate(struct clContext * C,
                                struct clProfile * srcProfile,
                                clTransformFormat srcFormat,
                                struct clProfile * dstProfile,
                                clTransformFormat dstFormat,
                                clTonemap tonemap);
void clTransformDestroy(struct clContext * C, clTransform * transform);
void clTransformPrepare(struct clContext * C, struct clTransform * transform);
clBool clTransformUsesCCMM(struct clContext * C, clTransform * transform);
const char * clTransformCMMName(struct clContext * C, clTransform * transform);    // Convenience function
float clTransformGetLuminanceScale(struct clContext * C, clTransform * transform); // Convenience function
void clTransformRun(struct clContext * C, clTransform * transform, float * srcPixels, float * dstPixels, int pixelCount);

// if X+Y+Z is 0, clTransformXYZToXYY() returns (whitePointX, whitePointY, 0)
void clTransformXYZToXYY(struct clContext * C, float * dstXYY, const float * srcXYZ, float whitePointX, float whitePointY);
void clTransformXYYToXYZ(struct clContext * C, float * dstXYZ, const float * srcXYY);

int clTransformCalcHLGLuminance(int diffuseWhite);
int clTransformCalcDefaultLuminanceFromHLG(int hlgLuminance);
float clTransformCalcMaxY(clContext * C, clTransform * linearFromXYZ, clTransform * linearToXYZ, float x, float y);
void clTransformDeriveXYZMatrix(struct clContext * C, struct clProfilePrimaries * primaries, gbMat3 * toXYZ);

float clTransformEOTF_PQ(float N);
float clTransformOETF_PQ(float L);

// define to debug transform matrix math in colorist-test
// #define DEBUG_MATRIX_MATH

#endif // ifndef COLORIST_TRANSFORM_H
