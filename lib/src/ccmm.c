#include "colorist/ccmm.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include "gb_math.h"

#include <string.h>

// ----------------------------------------------------------------------------
// Color Conversion Math

// From http://docs-hoffmann.de/ciexyz29082000.pdf, Section 11.4
static clBool deriveXYZMatrixAndGamma(struct clContext * C, struct clProfile * profile, gbMat3 * toXYZ, clBool * hasGamma, float * gamma)
{
    if (profile) {
        clProfilePrimaries primaries = { 0 };
        clProfileCurve curve = { 0 };
        int luminance = 0;

        gbVec3 U, W;
        gbMat3 P, PInv, D;

        if (!clProfileQuery(C, profile, &primaries, &curve, &luminance)) {
            clContextLogError(C, "deriveXYZMatrix: fatal error querying profile");
            return clFalse;
        }

        *gamma = curve.gamma;
        if (fabsf(curve.gamma - 1.0f) < 0.0001f) {
            *hasGamma = clFalse;
        } else {
            *hasGamma = clTrue;
        }

        P.col[0].x = primaries.red[0];
        P.col[0].y = primaries.red[1];
        P.col[0].z = 1 - primaries.red[0] - primaries.red[1];
        P.col[1].x = primaries.green[0];
        P.col[1].y = primaries.green[1];
        P.col[1].z = 1 - primaries.green[0] - primaries.green[1];
        P.col[2].x = primaries.blue[0];
        P.col[2].y = primaries.blue[1];
        P.col[2].z = 1 - primaries.blue[0] - primaries.blue[1];
        gb_mat3_inverse(&PInv, &P);

        W.x = primaries.white[0];
        W.y = primaries.white[1];
        W.z = 1 - primaries.white[0] - primaries.white[1];
        gb_mat3_mul_vec3(&U, &PInv, W);

        memset(&D, 0, sizeof(D));
        D.col[0].x = U.x / W.y;
        D.col[1].y = U.y / W.y;
        D.col[2].z = U.z / W.y;

        gb_mat3_mul(toXYZ, &P, &D);
    } else {
        // No profile; we're already XYZ!
        *hasGamma = clFalse;
        *gamma = 0.0f;
        gb_mat3_identity(toXYZ);
    }
    return clTrue;
}

// The real color conversion function
static void transformFloatToFloat(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        gbVec3 src;
        if (transform->srcHasGamma) {
            src.x = powf((srcPixel[0] >= 0.0f) ? srcPixel[0] : 0.0f, transform->srcGamma);
            src.y = powf((srcPixel[1] >= 0.0f) ? srcPixel[1] : 0.0f, transform->srcGamma);
            src.z = powf((srcPixel[2] >= 0.0f) ? srcPixel[2] : 0.0f, transform->srcGamma);
        } else {
            memcpy(&src, srcPixel, sizeof(src));
        }
        if (transform->dstHasGamma) {
            float tmp[3];
            gb_mat3_mul_vec3((gbVec3 *)tmp, &transform->matSrcToDst, src);
            dstPixel[0] = powf((tmp[0] >= 0.0f) ? tmp[0] : 0.0f, transform->dstInvGamma);
            dstPixel[1] = powf((tmp[1] >= 0.0f) ? tmp[1] : 0.0f, transform->dstInvGamma);
            dstPixel[2] = powf((tmp[2] >= 0.0f) ? tmp[2] : 0.0f, transform->dstInvGamma);
        } else {
            gb_mat3_mul_vec3((gbVec3 *)dstPixel, &transform->matSrcToDst, src);
        }
        if (dstPixelBytes > 12) {
            if (srcPixelBytes > 12) {
                // Copy alpha
                dstPixel[3] = srcPixel[3];
            } else {
                // Full alpha
                dstPixel[3] = 1.0f;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Transform wrappers for RGBA8 and RGBA16

static void transformFloatToRGBA8(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float maxChannel = 255.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        transformFloatToFloat(C, transform, (uint8_t *)srcPixel, srcPixelBytes, (uint8_t *)tmpPixel, dstPixelBytes, 1);
        dstPixel[0] = (uint8_t)clPixelMathRoundf(tmpPixel[0] * maxChannel);
        dstPixel[1] = (uint8_t)clPixelMathRoundf(tmpPixel[1] * maxChannel);
        dstPixel[2] = (uint8_t)clPixelMathRoundf(tmpPixel[2] * maxChannel);
        if ((srcPixelBytes > 12) && (dstPixelBytes > 3)) {
            // reformat alpha
            dstPixel[3] = (uint8_t)clPixelMathRoundf(tmpPixel[3] * maxChannel);
        } else if (srcPixelBytes > 12) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 3) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 255;
        }
    }
}

static void transformFloatToRGBA16(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float maxChannel = 65535.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        transformFloatToFloat(C, transform, (uint8_t *)srcPixel, srcPixelBytes, (uint8_t *)tmpPixel, dstPixelBytes, 1);
        dstPixel[0] = (uint16_t)clPixelMathRoundf(tmpPixel[0] * maxChannel);
        dstPixel[1] = (uint16_t)clPixelMathRoundf(tmpPixel[1] * maxChannel);
        dstPixel[2] = (uint16_t)clPixelMathRoundf(tmpPixel[2] * maxChannel);
        if ((srcPixelBytes > 12) && (dstPixelBytes > 6)) {
            // reformat alpha
            dstPixel[3] = (uint16_t)clPixelMathRoundf(tmpPixel[3] * maxChannel);
        } else if (srcPixelBytes > 12) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 6) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 65535;
        }
    }
}

static void transformRGBA8ToFloat(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float rescale = 1.0f / 255.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        tmpPixel[0] = (float)srcPixel[0] * rescale;
        tmpPixel[1] = (float)srcPixel[1] * rescale;
        tmpPixel[2] = (float)srcPixel[2] * rescale;
        if ((srcPixelBytes > 3) && (dstPixelBytes > 12)) {
            // reformat alpha
            tmpPixel[3] = (float)srcPixel[3] * rescale;
        } else if (srcPixelBytes > 3) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 12) {
            // RGB -> RGBA, set full opacity
            tmpPixel[3] = 1.0f;
        }
        transformFloatToFloat(C, transform, (uint8_t *)tmpPixel, dstPixelBytes, dstPixels, dstPixelBytes, 1);
    }
}

static void transformRGBA16ToFloat(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float rescale = 1.0f / 65535.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        tmpPixel[0] = (float)srcPixel[0] * rescale;
        tmpPixel[1] = (float)srcPixel[1] * rescale;
        tmpPixel[2] = (float)srcPixel[2] * rescale;
        if ((srcPixelBytes > 6) && (dstPixelBytes > 12)) {
            // reformat alpha
            tmpPixel[3] = (float)srcPixel[3] * rescale;
        } else if (srcPixelBytes > 6) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 12) {
            // RGB -> RGBA, set full opacity
            tmpPixel[3] = 1.0f;
        }
        transformFloatToFloat(C, transform, (uint8_t *)tmpPixel, dstPixelBytes, dstPixels, dstPixelBytes, 1);
    }
}

static void transformRGBA8ToRGBA16(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    const float srcRescale = 1.0f / 255.0f;
    const float dstRescale = 65535.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float tmpSrc[4];
        float tmpDst[4];
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        int tmpSrcBytes;
        int tmpDstBytes;

        tmpSrc[0] = (float)srcPixel[0] * srcRescale;
        tmpSrc[1] = (float)srcPixel[1] * srcRescale;
        tmpSrc[2] = (float)srcPixel[2] * srcRescale;
        if (srcPixelBytes > 3) {
            tmpSrc[3] = (float)srcPixel[3] * srcRescale;
            tmpSrcBytes = 16;
        } else {
            tmpSrcBytes = 12;
        }
        if (dstPixelBytes > 6) {
            tmpDstBytes = 16;
        } else {
            tmpDstBytes = 12;
        }

        transformFloatToFloat(C, transform, (uint8_t *)tmpSrc, tmpSrcBytes, (uint8_t *)tmpDst, tmpDstBytes, 1);

        dstPixel[0] = (uint16_t)clPixelMathRoundf((float)tmpDst[0] * dstRescale);
        dstPixel[1] = (uint16_t)clPixelMathRoundf((float)tmpDst[1] * dstRescale);
        dstPixel[2] = (uint16_t)clPixelMathRoundf((float)tmpDst[2] * dstRescale);
        if ((srcPixelBytes > 3) && (dstPixelBytes > 6)) {
            // reformat alpha
            dstPixel[3] = (uint16_t)clPixelMathRoundf((float)tmpDst[3] * dstRescale);
        } else if (srcPixelBytes > 3) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 6) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 65535;
        }
    }
}

static void transformRGBA16ToRGBA8(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    const float srcRescale = 1.0f / 65535.0f;
    const float dstRescale = 255.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float tmpSrc[4];
        float tmpDst[4];
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        int tmpSrcBytes;
        int tmpDstBytes;

        tmpSrc[0] = (float)srcPixel[0] * srcRescale;
        tmpSrc[1] = (float)srcPixel[1] * srcRescale;
        tmpSrc[2] = (float)srcPixel[2] * srcRescale;
        if (srcPixelBytes > 6) {
            tmpSrc[3] = (float)srcPixel[3] * srcRescale;
            tmpSrcBytes = 16;
        } else {
            tmpSrcBytes = 12;
        }
        if (dstPixelBytes > 3) {
            tmpDstBytes = 16;
        } else {
            tmpDstBytes = 12;
        }

        transformFloatToFloat(C, transform, (uint8_t *)tmpSrc, tmpSrcBytes, (uint8_t *)tmpDst, tmpDstBytes, 1);

        dstPixel[0] = (uint8_t)clPixelMathRoundf((float)tmpDst[0] * dstRescale);
        dstPixel[1] = (uint8_t)clPixelMathRoundf((float)tmpDst[1] * dstRescale);
        dstPixel[2] = (uint8_t)clPixelMathRoundf((float)tmpDst[2] * dstRescale);
        if ((srcPixelBytes > 6) && (dstPixelBytes > 3)) {
            // reformat alpha
            dstPixel[3] = (uint8_t)clPixelMathRoundf((float)tmpDst[3] * dstRescale);
        } else if (srcPixelBytes > 6) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 3) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 255;
        }
    }
}

// ----------------------------------------------------------------------------
// Reformatting

static void reformatFloatToFloat(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        memcpy(dstPixel, srcPixel, sizeof(float) * 3); // all float formats are at least 3 floats
        if ((srcPixelBytes > 12) && (dstPixelBytes > 12)) {
            COLORIST_FAILURE("reformatFloatToFloat: unnecessary reformat (alpha)");
            dstPixel[3] = srcPixel[3];
        } else if (srcPixelBytes > 12) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 12) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 1.0f;
        } else {
            COLORIST_FAILURE("reformatFloatToFloat: unnecessary reformat (no alpha)");
        }
    }
}

static void reformatFloatToRGBA8(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float maxChannel = 255.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint8_t)clPixelMathRoundf(srcPixel[0] * maxChannel);
        dstPixel[1] = (uint8_t)clPixelMathRoundf(srcPixel[1] * maxChannel);
        dstPixel[2] = (uint8_t)clPixelMathRoundf(srcPixel[2] * maxChannel);
        if ((srcPixelBytes > 12) && (dstPixelBytes > 3)) {
            // reformat alpha
            dstPixel[3] = (uint8_t)clPixelMathRoundf(srcPixel[3] * maxChannel);
        } else if (srcPixelBytes > 12) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 3) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 255;
        }
    }
}

static void reformatFloatToRGBA16(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float maxChannel = 65535.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint16_t)clPixelMathRoundf(srcPixel[0] * maxChannel);
        dstPixel[1] = (uint16_t)clPixelMathRoundf(srcPixel[1] * maxChannel);
        dstPixel[2] = (uint16_t)clPixelMathRoundf(srcPixel[2] * maxChannel);
        if ((srcPixelBytes > 12) && (dstPixelBytes > 6)) {
            // reformat alpha
            dstPixel[3] = (uint16_t)clPixelMathRoundf(srcPixel[3] * maxChannel);
        } else if (srcPixelBytes > 12) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 6) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 65535;
        }
    }
}

static void reformatRGBA8ToFloat(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float rescale = 1.0f / 255.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (float)srcPixel[0] * rescale;
        dstPixel[1] = (float)srcPixel[1] * rescale;
        dstPixel[2] = (float)srcPixel[2] * rescale;
        if ((srcPixelBytes > 3) && (dstPixelBytes > 12)) {
            // reformat alpha
            dstPixel[3] = (float)srcPixel[3] * rescale;
        } else if (srcPixelBytes > 3) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 12) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 1.0f;
        }
    }
}

static void reformatRGBA16ToFloat(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float rescale = 1.0f / 65535.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (float)srcPixel[0] * rescale;
        dstPixel[1] = (float)srcPixel[1] * rescale;
        dstPixel[2] = (float)srcPixel[2] * rescale;
        if ((srcPixelBytes > 6) && (dstPixelBytes > 12)) {
            // reformat alpha
            dstPixel[3] = (float)srcPixel[3] * rescale;
        } else if (srcPixelBytes > 6) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 12) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 1.0f;
        }
    }
}

static void reformatRGBA8ToRGBA16(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    const float rescale = 65535.0f / 255.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint16_t)clPixelMathRoundf((float)srcPixel[0] * rescale);
        dstPixel[1] = (uint16_t)clPixelMathRoundf((float)srcPixel[1] * rescale);
        dstPixel[2] = (uint16_t)clPixelMathRoundf((float)srcPixel[2] * rescale);
        if ((srcPixelBytes > 3) && (dstPixelBytes > 6)) {
            // reformat alpha
            dstPixel[3] = (uint16_t)clPixelMathRoundf((float)srcPixel[3] * rescale);
        } else if (srcPixelBytes > 3) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 6) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 65535;
        }
    }
}

static void reformatRGBA16ToRGBA8(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    const float rescale = 255.0f / 65535.0f;
    int i;
    for (i = 0; i < pixelCount; ++i) {
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint8_t)clPixelMathRoundf((float)srcPixel[0] * rescale);
        dstPixel[1] = (uint8_t)clPixelMathRoundf((float)srcPixel[1] * rescale);
        dstPixel[2] = (uint8_t)clPixelMathRoundf((float)srcPixel[2] * rescale);
        if ((srcPixelBytes > 6) && (dstPixelBytes > 3)) {
            // reformat alpha
            dstPixel[3] = (uint8_t)clPixelMathRoundf((float)srcPixel[3] * rescale);
        } else if (srcPixelBytes > 6) {
            // RGBA -> RGB, nothing to do
        } else if (dstPixelBytes > 3) {
            // RGB -> RGBA, set full opacity
            dstPixel[3] = 255;
        }
    }
}

// ----------------------------------------------------------------------------
// Transform API

void clCCMMPrepareTransform(struct clContext * C, struct clTransform * transform)
{
    if (!transform->ccmmReady) {
        gbMat3 srcToXYZ;
        gbMat3 dstToXYZ;
        gbMat3 XYZtoDst;

        deriveXYZMatrixAndGamma(C, transform->srcProfile, &srcToXYZ, &transform->srcHasGamma, &transform->srcGamma);
        deriveXYZMatrixAndGamma(C, transform->dstProfile, &dstToXYZ, &transform->dstHasGamma, &transform->dstInvGamma);
        if (transform->dstHasGamma && (transform->dstInvGamma != 0.0f)) {
            transform->dstInvGamma = 1.0f / transform->dstInvGamma;
        }
        gb_mat3_inverse(&XYZtoDst, &dstToXYZ);
        gb_mat3_transpose(&XYZtoDst);
        gb_mat3_mul(&transform->matSrcToDst, &srcToXYZ, &XYZtoDst);
        gb_mat3_transpose(&transform->matSrcToDst);

        transform->ccmmReady = clTrue;
    }
}

void clCCMMTransform(struct clContext * C, struct clTransform * transform, void * srcPixels, void * dstPixels, int pixelCount)
{
    int srcPixelBytes = clTransformFormatToPixelBytes(C, transform->srcFormat);
    int dstPixelBytes = clTransformFormatToPixelBytes(C, transform->dstFormat);

    COLORIST_ASSERT(!transform->srcProfile || transform->srcProfile->ccmm);
    COLORIST_ASSERT(!transform->dstProfile || transform->dstProfile->ccmm);

    if (!transform->ccmmReady) {
        clContextLogError(C, "clCCMMTransform called without a call to clCCMMPrepareTransform");
        return;
    }

    // After this point, find a single valid return point from this function, or die

    if (clProfileMatches(C, transform->srcProfile, transform->dstProfile)) {
        // No color conversion necessary, just format conversion

        if (transform->srcFormat == transform->dstFormat) {
            // Everything is identical, no format conversion, just memcpy
            memcpy(dstPixels, srcPixels, srcPixelBytes * pixelCount);
            return;
        }

        if (clTransformFormatIsFloat(C, transform->srcFormat) && clTransformFormatIsFloat(C, transform->dstFormat)) {
            // Float to Float, losing or gaining alpha
            reformatFloatToFloat(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
            return;
        } else if (clTransformFormatIsFloat(C, transform->srcFormat)) {
            // Float -> 8 or 16
            if (transform->dstFormat == CL_TF_RGBA_8) {
                reformatFloatToRGBA8(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            } else {
                reformatFloatToRGBA16(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        } else if (clTransformFormatIsFloat(C, transform->dstFormat)) {
            // 8 or 16 -> Float
            if (transform->srcFormat == CL_TF_RGBA_8) {
                reformatRGBA8ToFloat(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            } else {
                reformatRGBA16ToFloat(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        } else {
            // 8 or 16 -> 8 or 16
            if (transform->srcFormat == CL_TF_RGBA_8) {
                reformatRGBA8ToRGBA16(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            } else {
                reformatRGBA16ToRGBA8(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        }
    } else {
        // Color conversion is required

        if (clTransformFormatIsFloat(C, transform->srcFormat) && clTransformFormatIsFloat(C, transform->dstFormat)) {
            // Float to Float
            transformFloatToFloat(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
            return;
        } else if (clTransformFormatIsFloat(C, transform->srcFormat)) {
            // Float -> 8 or 16
            if (transform->dstFormat == CL_TF_RGBA_8) {
                transformFloatToRGBA8(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            } else {
                transformFloatToRGBA16(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        } else if (clTransformFormatIsFloat(C, transform->dstFormat)) {
            // 8 or 16 -> Float
            if (transform->srcFormat == CL_TF_RGBA_8) {
                transformRGBA8ToFloat(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            } else {
                transformRGBA16ToFloat(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        } else {
            // 8 or 16 -> 8 or 16
            if (transform->srcFormat == CL_TF_RGBA_8) {
                transformRGBA8ToRGBA16(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            } else {
                transformRGBA16ToRGBA8(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        }
    }

    COLORIST_FAILURE("clCCMMTransform: Failed to find correct conversion method");
}
