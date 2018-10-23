#include "colorist/ccmm.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include "gb_math.h"

#include <string.h>

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

void transformFloatToFloat(struct clContext * C, struct clTransform * transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
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
        if (dstPixel[0] < 0.0f)
            dstPixel[0] = 0.0f;
        if (dstPixel[1] < 0.0f)
            dstPixel[1] = 0.0f;
        if (dstPixel[2] < 0.0f)
            dstPixel[2] = 0.0f;
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
        return;
    }

    // TODO: Add support for all formats
    COLORIST_ASSERT((transform->srcFormat == CL_TF_XYZ_FLOAT) || (transform->srcFormat == CL_TF_RGB_FLOAT) || (transform->srcFormat == CL_TF_RGBA_FLOAT));
    COLORIST_ASSERT((transform->dstFormat == CL_TF_XYZ_FLOAT) || (transform->dstFormat == CL_TF_RGB_FLOAT) || (transform->dstFormat == CL_TF_RGBA_FLOAT));
    transformFloatToFloat(C, transform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
}
