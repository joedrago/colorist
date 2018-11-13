#include "colorist/transform.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/task.h"

#include "gb_math.h"

#include <string.h>

// The small amount after the 1.0 here buys us  a little imprecision wiggle
// room on an automatic tonemap. (It's ok to clip if our luminance scale is
// this close.)
#define AUTO_TONEMAP_LUMINANCE_SCALE_THRESHOLD (1.001f)

#define SRC_8_HAS_ALPHA() (srcPixelBytes > 3)
#define SRC_16_HAS_ALPHA() (srcPixelBytes > 7)
#define SRC_FLOAT_HAS_ALPHA() (srcPixelBytes > 15)

#define DST_8_HAS_ALPHA() (dstPixelBytes > 3)
#define DST_16_HAS_ALPHA() (dstPixelBytes > 7)
#define DST_FLOAT_HAS_ALPHA() (dstPixelBytes > 15)

static cmsUInt32Number clTransformFormatToLCMSFormat(struct clContext * C, clTransformFormat format);

// ----------------------------------------------------------------------------
// Debug Helpers

#if defined(DEBUG_MATRIX_MATH)
static void DEBUG_PRINT_MATRIX(const char * name, gbMat3 * m)
{
    printf("mat: %s\n", name);
    printf("  %g    %g    %g\n", m->x.x, m->y.x, m->z.x);
    printf("  %g    %g    %g\n", m->x.y, m->y.y, m->z.y);
    printf("  %g    %g    %g\n", m->x.z, m->y.z, m->z.z);
}

static void DEBUG_PRINT_VECTOR(const char * name, gbVec3 * v)
{
    printf("vec: %s\n", name);
    printf("  %g    %g    %g\n", v->x, v->y, v->z);
}
#else
#define DEBUG_PRINT_MATRIX(NAME, M)
#define DEBUG_PRINT_VECTOR(NAME, V)
#endif

// ----------------------------------------------------------------------------
// Color Conversion Math

// From http://docs-hoffmann.de/ciexyz29082000.pdf, Section 11.4
static void deriveXYZMatrix(struct clContext * C, clProfilePrimaries * primaries, gbMat3 * toXYZ)
{
    COLORIST_UNUSED(C);

    gbVec3 U, W;
    gbMat3 P, PInv, D;

    P.col[0].x = primaries->red[0];
    P.col[0].y = primaries->red[1];
    P.col[0].z = 1 - primaries->red[0] - primaries->red[1];
    P.col[1].x = primaries->green[0];
    P.col[1].y = primaries->green[1];
    P.col[1].z = 1 - primaries->green[0] - primaries->green[1];
    P.col[2].x = primaries->blue[0];
    P.col[2].y = primaries->blue[1];
    P.col[2].z = 1 - primaries->blue[0] - primaries->blue[1];
    DEBUG_PRINT_MATRIX("P", &P);

    gb_mat3_inverse(&PInv, &P);
    DEBUG_PRINT_MATRIX("PInv", &PInv);

    W.x = primaries->white[0];
    W.y = primaries->white[1];
    W.z = 1 - primaries->white[0] - primaries->white[1];
    DEBUG_PRINT_VECTOR("W", &W);

    gb_mat3_mul_vec3(&U, &PInv, W);
    DEBUG_PRINT_VECTOR("U", &U);

    memset(&D, 0, sizeof(D));
    D.col[0].x = U.x / W.y;
    D.col[1].y = U.y / W.y;
    D.col[2].z = U.z / W.y;
    DEBUG_PRINT_MATRIX("D", &D);

    gb_mat3_mul(toXYZ, &P, &D);
    gb_mat3_transpose(toXYZ);
    DEBUG_PRINT_MATRIX("Cxr", toXYZ);
}

static clBool derivePrimariesAndXTF(struct clContext * C, struct clProfile * profile, clProfilePrimaries * outPrimaries, clTransformTransferFunction * outXTF, float * outGamma)
{
    if (profile) {
        clProfileCurve curve;
        int luminance = 0;

        if (clProfileHasPQSignature(C, profile, outPrimaries)) {
            *outXTF = CL_XTF_PQ;
            *outGamma = 0.0f;
        } else if (clProfileQuery(C, profile, outPrimaries, &curve, &luminance)) {
            *outXTF = CL_XTF_GAMMA;
            *outGamma = curve.gamma;
        } else {
            clContextLogError(C, "deriveXYZMatrix: fatal error querying profile");
            return clFalse;
        }
    } else {
        // No profile; we're already XYZ!
        memset(outPrimaries, 0, sizeof(clProfilePrimaries));
        *outXTF = CL_XTF_NONE;
        *outGamma = 0.0f;
    }
    return clTrue;
}

void clTransformPrepare(struct clContext * C, struct clTransform * transform)
{
    clBool useCCMM = clTransformUsesCCMM(C, transform);
    if ((useCCMM && !transform->ccmmReady) || (!useCCMM && !transform->lcmsReady)) {
        // Calculate luminance scaling

        // Default to D65, allow either profile to override it, with the priority: dst > src > D65
        transform->whitePointX = 0.3127f;
        transform->whitePointY = 0.3290f;

        if (transform->srcProfile) {
            clProfilePrimaries srcPrimaries;
            clProfileCurve srcCurve;
            int srcLuminance;

            clProfileQuery(C, transform->srcProfile, &srcPrimaries, &srcCurve, &srcLuminance);
            srcLuminance = (srcLuminance != 0) ? srcLuminance : COLORIST_DEFAULT_LUMINANCE;
            transform->srcLuminanceScale = (float)srcLuminance;
            transform->srcCurveScale = srcCurve.implicitScale;
            transform->whitePointX = srcPrimaries.white[0];
            transform->whitePointY = srcPrimaries.white[1];
        } else {
            transform->srcLuminanceScale = 1.0f;
            transform->srcCurveScale = 1.0f;
        }

        if (transform->dstProfile) {
            clProfilePrimaries dstPrimaries;
            clProfileCurve dstCurve;
            int dstLuminance;

            clProfileQuery(C, transform->dstProfile, &dstPrimaries, &dstCurve, &dstLuminance);
            dstLuminance = (dstLuminance != 0) ? dstLuminance : COLORIST_DEFAULT_LUMINANCE;
            transform->dstLuminanceScale = (float)dstLuminance;
            transform->dstCurveScale = dstCurve.implicitScale;
            transform->whitePointX = dstPrimaries.white[0];
            transform->whitePointY = dstPrimaries.white[1];
        } else {
            transform->dstLuminanceScale = 1.0f;
            transform->dstCurveScale = 1.0f;
        }

        switch (transform->tonemap) {
            case CL_TONEMAP_AUTO:
                transform->tonemapEnabled = (((transform->srcLuminanceScale * transform->srcCurveScale) / (transform->dstLuminanceScale * transform->dstCurveScale)) > AUTO_TONEMAP_LUMINANCE_SCALE_THRESHOLD) ? clTrue : clFalse;
                break;
            case CL_TONEMAP_ON:
                transform->tonemapEnabled = clTrue;
                break;
            case CL_TONEMAP_OFF:
                transform->tonemapEnabled = clFalse;
                break;
        }

        if (transform->tonemapEnabled || (fabsf((transform->srcLuminanceScale * transform->srcCurveScale) - (transform->dstLuminanceScale * transform->dstCurveScale)) > 0.00001f)) {
            transform->luminanceScaleEnabled = clTrue;
        } else {
            transform->luminanceScaleEnabled = clFalse;
        }
    }

    if (useCCMM) {
        // Prepare CCMM
        if (!transform->ccmmReady) {
            clProfilePrimaries srcPrimaries;
            clProfilePrimaries dstPrimaries;
            gbMat3 dstToXYZ;

            derivePrimariesAndXTF(C, transform->srcProfile, &srcPrimaries, &transform->ccmmSrcEOTF, &transform->ccmmSrcGamma);
            derivePrimariesAndXTF(C, transform->dstProfile, &dstPrimaries, &transform->ccmmDstOETF, &transform->ccmmDstInvGamma);

            if (clProfilePrimariesMatch(C, &srcPrimaries, &dstPrimaries)) {
                // if the src/dst primaries are close enough, make them match exactly to help roundtripping
                // by making the SrcToXYZ and XYZtoDst matrices as close to true inverses of one another as possible.
                memcpy(&srcPrimaries, &dstPrimaries, sizeof(srcPrimaries));
            }

            if (transform->srcProfile) {
                deriveXYZMatrix(C, &srcPrimaries, &transform->ccmmSrcToXYZ);
            } else {
                gb_mat3_identity(&transform->ccmmSrcToXYZ);
            }
            if (transform->dstProfile) {
                deriveXYZMatrix(C, &dstPrimaries, &dstToXYZ);
            } else {
                gb_mat3_identity(&dstToXYZ);
            }
            if ((transform->ccmmDstOETF == CL_XTF_GAMMA) && (transform->ccmmDstInvGamma != 0.0f)) {
                transform->ccmmDstInvGamma = 1.0f / transform->ccmmDstInvGamma;
            }
            gb_mat3_inverse(&transform->ccmmXYZToDst, &dstToXYZ);
            gb_mat3_transpose(&transform->ccmmXYZToDst);

            DEBUG_PRINT_MATRIX("XYZtoDst", &transform->ccmmXYZToDst);
            DEBUG_PRINT_MATRIX("MA", &transform->ccmmSrcToXYZ);
            DEBUG_PRINT_MATRIX("MB", &transform->ccmmXYZToDst);
            gb_mat3_mul(&transform->ccmmCombined, &transform->ccmmSrcToXYZ, &transform->ccmmXYZToDst);
            DEBUG_PRINT_MATRIX("MA*MB", &transform->ccmmCombined);

            transform->ccmmReady = clTrue;
        }
    } else {
        // Prepare LittleCMS
        if (!transform->lcmsReady) {
            cmsUInt32Number srcFormat = clTransformFormatToLCMSFormat(C, transform->srcFormat);
            cmsUInt32Number dstFormat = clTransformFormatToLCMSFormat(C, transform->dstFormat);
            cmsHPROFILE srcProfileHandle;
            cmsHPROFILE dstProfileHandle;

            transform->lcmsXYZProfile = cmsCreateXYZProfileTHR(C->lcms);

            // Choose src profile handle
            if (transform->srcProfile) {
                srcProfileHandle = transform->srcProfile->handle;
            } else {
                srcProfileHandle = transform->lcmsXYZProfile;
            }

            // Choose dst profile handle
            if (transform->dstProfile) {
                dstProfileHandle = transform->dstProfile->handle;
            } else {
                dstProfileHandle = transform->lcmsXYZProfile;
            }

            transform->lcmsSrcToXYZ = cmsCreateTransformTHR(C->lcms,
                srcProfileHandle, srcFormat,
                transform->lcmsXYZProfile, TYPE_XYZ_FLT,
                INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

            transform->lcmsXYZToDst = cmsCreateTransformTHR(C->lcms,
                transform->lcmsXYZProfile, TYPE_XYZ_FLT,
                dstProfileHandle, dstFormat,
                INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

            transform->lcmsCombined = cmsCreateTransformTHR(C->lcms,
                srcProfileHandle, srcFormat,
                dstProfileHandle, dstFormat,
                INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

            transform->lcmsReady = clTrue;
        }
    }
}

// SMPTE ST.2084: https://ieeexplore.ieee.org/servlet/opac?punumber=7291450

static const float PQ_C1 = 0.8359375;       // 3424.0 / 4096.0
static const float PQ_C2 = 18.8515625;      // 2413.0 / 4096.0 * 32.0
static const float PQ_C3 = 18.6875;         // 2392.0 / 4096.0 * 32.0
static const float PQ_M1 = 0.1593017578125; // 2610.0 / 4096.0 / 4.0
static const float PQ_M2 = 78.84375;        // 2523.0 / 4096.0 * 128.0

// SMPTE ST.2084: Equation 4.1
// L = ( (max(N^(1/m2) - c1, 0)) / (c2 - c3*N^(1/m2)) )^(1/m1)
static float PQ_EOTF(float N)
{
    float N1m2 = powf(N, 1 / PQ_M2);
    float N1m2c1 = N1m2 - PQ_C1;
    if (N1m2c1 < 0.0f)
        N1m2c1 = 0.0f;
    float c2c3N1m2 = PQ_C2 - (PQ_C3 * N1m2);
    return powf(N1m2c1 / c2c3N1m2, 1 / PQ_M1);
}

// SMPTE ST.2084: Equation 5.2
// N = ( (c1 + (c2 * L^m1)) / (1 + (c3 * L^m1)) )^m2
static float PQ_OETF(float L)
{
    float Lm1 = powf(L, PQ_M1);
    float c2Lm1 = PQ_C2 * Lm1;
    float c3Lm1 = PQ_C3 * Lm1;
    return powf((PQ_C1 + c2Lm1) / (1 + c3Lm1), PQ_M2);
}

// The real color conversion function
static void transformFloatToFloat(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    for (int i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        gbVec3 src;
        float XYZ[3];

        if (useCCMM) {
            switch (transform->ccmmSrcEOTF) {
                default:
                case CL_XTF_NONE:
                    memcpy(&src, srcPixel, sizeof(src));
                    break;
                case CL_XTF_GAMMA:
                    src.x = powf((srcPixel[0] >= 0.0f) ? srcPixel[0] : 0.0f, transform->ccmmSrcGamma);
                    src.y = powf((srcPixel[1] >= 0.0f) ? srcPixel[1] : 0.0f, transform->ccmmSrcGamma);
                    src.z = powf((srcPixel[2] >= 0.0f) ? srcPixel[2] : 0.0f, transform->ccmmSrcGamma);
                    break;
                case CL_XTF_PQ:
                    src.x = PQ_EOTF((srcPixel[0] >= 0.0f) ? srcPixel[0] : 0.0f);
                    src.y = PQ_EOTF((srcPixel[1] >= 0.0f) ? srcPixel[1] : 0.0f);
                    src.z = PQ_EOTF((srcPixel[2] >= 0.0f) ? srcPixel[2] : 0.0f);
                    break;
            }

            gb_mat3_mul_vec3((gbVec3 *)XYZ, &transform->ccmmSrcToXYZ, src);
        } else {
            // Use LCMS
            cmsDoTransform(transform->lcmsSrcToXYZ, srcPixel, XYZ, 1);
        }

        // if tonemapping is necessary, luminance scale MUST be enabled
        COLORIST_ASSERT(!transform->tonemapEnabled || transform->luminanceScaleEnabled);

        if (transform->luminanceScaleEnabled) {
            float xyY[3];

            // Convert to xyY
            clTransformXYZToXYY(C, xyY, XYZ, transform->whitePointX, transform->whitePointY);

            // Apply srcCurveScale as CCMM, if any (LCMS implicitly does this)
            if (useCCMM) {
                xyY[2] *= transform->srcCurveScale;
            }

            // Luminance scale
            xyY[2] *= transform->srcLuminanceScale;
            xyY[2] /= transform->dstLuminanceScale;

            // Apply inverse dstCurveScale prior to tonemapping to ensure tonemap gets [0-1] range
            xyY[2] /= transform->dstCurveScale;

            // Tonemap
            if (transform->tonemapEnabled) {
                // reinhard tonemap
                xyY[2] = xyY[2] / (1.0f + xyY[2]);
            }

            if (!useCCMM) {
                // Re-apply dst scale for LCMS as it expects the XYZ->Dst input to be overranged
                xyY[2] *= transform->dstCurveScale;
            }

            // Convert to XYZ
            clTransformXYYToXYZ(C, XYZ, xyY);
        }

        if (useCCMM) {
            float tmp[3];
            memcpy(&src, XYZ, sizeof(src));

            switch (transform->ccmmDstOETF) {
                case CL_XTF_NONE:
                    gb_mat3_mul_vec3((gbVec3 *)dstPixel, &transform->ccmmXYZToDst, src);
                    if (transform->dstProfile) {                         // don't clamp XYZ
                        dstPixel[0] = CL_CLAMP(dstPixel[0], 0.0f, 1.0f); // clamp
                        dstPixel[1] = CL_CLAMP(dstPixel[1], 0.0f, 1.0f); // clamp
                        dstPixel[2] = CL_CLAMP(dstPixel[2], 0.0f, 1.0f); // clamp
                    }
                    break;
                case CL_XTF_GAMMA:
                    gb_mat3_mul_vec3((gbVec3 *)tmp, &transform->ccmmXYZToDst, src);
                    if (transform->dstProfile) {               // don't clamp XYZ
                        tmp[0] = CL_CLAMP(tmp[0], 0.0f, 1.0f); // clamp
                        tmp[1] = CL_CLAMP(tmp[1], 0.0f, 1.0f); // clamp
                        tmp[2] = CL_CLAMP(tmp[2], 0.0f, 1.0f); // clamp
                    }
                    dstPixel[0] = powf((tmp[0] >= 0.0f) ? tmp[0] : 0.0f, transform->ccmmDstInvGamma);
                    dstPixel[1] = powf((tmp[1] >= 0.0f) ? tmp[1] : 0.0f, transform->ccmmDstInvGamma);
                    dstPixel[2] = powf((tmp[2] >= 0.0f) ? tmp[2] : 0.0f, transform->ccmmDstInvGamma);
                    break;
                case CL_XTF_PQ:
                    gb_mat3_mul_vec3((gbVec3 *)tmp, &transform->ccmmXYZToDst, src);
                    if (transform->dstProfile) {               // don't clamp XYZ
                        tmp[0] = CL_CLAMP(tmp[0], 0.0f, 1.0f); // clamp
                        tmp[1] = CL_CLAMP(tmp[1], 0.0f, 1.0f); // clamp
                        tmp[2] = CL_CLAMP(tmp[2], 0.0f, 1.0f); // clamp
                    }
                    dstPixel[0] = PQ_OETF((tmp[0] >= 0.0f) ? tmp[0] : 0.0f);
                    dstPixel[1] = PQ_OETF((tmp[1] >= 0.0f) ? tmp[1] : 0.0f);
                    dstPixel[2] = PQ_OETF((tmp[2] >= 0.0f) ? tmp[2] : 0.0f);
                    break;
            }
        } else {
            // LittleCMS
            cmsDoTransform(transform->lcmsXYZToDst, XYZ, dstPixel, 1);
            if (transform->dstProfile) {                         // don't clamp XYZ
                dstPixel[0] = CL_CLAMP(dstPixel[0], 0.0f, 1.0f); // clamp
                dstPixel[1] = CL_CLAMP(dstPixel[1], 0.0f, 1.0f); // clamp
                dstPixel[2] = CL_CLAMP(dstPixel[2], 0.0f, 1.0f); // clamp
            }
        }

        if (DST_FLOAT_HAS_ALPHA()) {
            if (SRC_FLOAT_HAS_ALPHA()) {
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
// Transform wrappers for RGB/RGBA

static void transformFloatToRGB8(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float maxChannel = 255.0f;

    for (int i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)srcPixel, srcPixelBytes, (uint8_t *)tmpPixel, dstPixelBytes, 1);
        dstPixel[0] = (uint8_t)clPixelMathRoundNormalized(tmpPixel[0], maxChannel);
        dstPixel[1] = (uint8_t)clPixelMathRoundNormalized(tmpPixel[1], maxChannel);
        dstPixel[2] = (uint8_t)clPixelMathRoundNormalized(tmpPixel[2], maxChannel);
        if (DST_8_HAS_ALPHA()) {
            if (SRC_FLOAT_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint8_t)clPixelMathRoundNormalized(tmpPixel[3], maxChannel);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 255;
            }
        }
    }
}

static void transformFloatToRGB16(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int dstDepth, int pixelCount)
{
    const int dstMaxChannel = (1 << dstDepth) - 1;
    const float dstRescale = (float)dstMaxChannel;

    for (int i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)srcPixel, srcPixelBytes, (uint8_t *)tmpPixel, dstPixelBytes, 1);
        dstPixel[0] = (uint16_t)clPixelMathRoundNormalized(tmpPixel[0], dstRescale);
        dstPixel[1] = (uint16_t)clPixelMathRoundNormalized(tmpPixel[1], dstRescale);
        dstPixel[2] = (uint16_t)clPixelMathRoundNormalized(tmpPixel[2], dstRescale);
        if (DST_16_HAS_ALPHA()) {
            if (SRC_FLOAT_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint16_t)clPixelMathRoundNormalized(tmpPixel[3], dstRescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = (uint16_t)dstMaxChannel;
            }
        }
    }
}

static void transformRGB8ToFloat(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    static const float srcRescale = 1.0f / 255.0f;

    for (int i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        tmpPixel[0] = (float)srcPixel[0] * srcRescale;
        tmpPixel[1] = (float)srcPixel[1] * srcRescale;
        tmpPixel[2] = (float)srcPixel[2] * srcRescale;
        if (DST_FLOAT_HAS_ALPHA()) {
            if (SRC_8_HAS_ALPHA()) {
                // reformat alpha
                tmpPixel[3] = (float)srcPixel[3] * srcRescale;
            } else {
                // RGB -> RGBA, set full opacity
                tmpPixel[3] = 1.0f;
            }
        }
        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)tmpPixel, dstPixelBytes, dstPixels, dstPixelBytes, 1);
    }
}

static void transformRGB16ToFloat(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, int srcDepth, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    const float srcRescale = 1.0f / (float)((1 << srcDepth) - 1);

    for (int i = 0; i < pixelCount; ++i) {
        float tmpPixel[4];
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        tmpPixel[0] = (float)srcPixel[0] * srcRescale;
        tmpPixel[1] = (float)srcPixel[1] * srcRescale;
        tmpPixel[2] = (float)srcPixel[2] * srcRescale;
        if (DST_FLOAT_HAS_ALPHA()) {
            if (SRC_16_HAS_ALPHA()) {
                // reformat alpha
                tmpPixel[3] = (float)srcPixel[3] * srcRescale;
            } else {
                // RGB -> RGBA, set full opacity
                tmpPixel[3] = 1.0f;
            }
        }
        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)tmpPixel, dstPixelBytes, dstPixels, dstPixelBytes, 1);
    }
}

static void transformRGB8ToRGB8(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    const float srcRescale = 1.0f / 255.0f;
    const float dstRescale = 255.0f;

    for (int i = 0; i < pixelCount; ++i) {
        float tmpSrc[4];
        float tmpDst[4];
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        int tmpSrcBytes;
        int tmpDstBytes;

        tmpSrc[0] = (float)srcPixel[0] * srcRescale;
        tmpSrc[1] = (float)srcPixel[1] * srcRescale;
        tmpSrc[2] = (float)srcPixel[2] * srcRescale;
        if (SRC_8_HAS_ALPHA()) {
            tmpSrc[3] = (float)srcPixel[3] * srcRescale;
            tmpSrcBytes = 16;
        } else {
            tmpSrcBytes = 12;
        }
        if (DST_8_HAS_ALPHA()) {
            tmpDstBytes = 16;
        } else {
            tmpDstBytes = 12;
        }

        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)tmpSrc, tmpSrcBytes, (uint8_t *)tmpDst, tmpDstBytes, 1);

        dstPixel[0] = (uint8_t)clPixelMathRoundNormalized(tmpDst[0], dstRescale);
        dstPixel[1] = (uint8_t)clPixelMathRoundNormalized(tmpDst[1], dstRescale);
        dstPixel[2] = (uint8_t)clPixelMathRoundNormalized(tmpDst[2], dstRescale);
        if (DST_8_HAS_ALPHA()) {
            if (SRC_8_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint8_t)clPixelMathRoundNormalized(tmpDst[3], dstRescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 255;
            }
        }
    }
}

static void transformRGB8ToRGB16(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int dstDepth, int pixelCount)
{
    static const float srcRescale = 1.0f / 255.0f;
    const int dstMaxChannel = (1 << dstDepth) - 1;
    const float dstRescale = (float)dstMaxChannel;

    for (int i = 0; i < pixelCount; ++i) {
        float tmpSrc[4];
        float tmpDst[4];
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        int tmpSrcBytes;
        int tmpDstBytes;

        tmpSrc[0] = (float)srcPixel[0] * srcRescale;
        tmpSrc[1] = (float)srcPixel[1] * srcRescale;
        tmpSrc[2] = (float)srcPixel[2] * srcRescale;
        if (SRC_8_HAS_ALPHA()) {
            tmpSrc[3] = (float)srcPixel[3] * srcRescale;
            tmpSrcBytes = 16;
        } else {
            tmpSrcBytes = 12;
        }
        if (DST_16_HAS_ALPHA()) {
            tmpDstBytes = 16;
        } else {
            tmpDstBytes = 12;
        }

        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)tmpSrc, tmpSrcBytes, (uint8_t *)tmpDst, tmpDstBytes, 1);

        dstPixel[0] = (uint16_t)clPixelMathRoundNormalized(tmpDst[0], dstRescale);
        dstPixel[1] = (uint16_t)clPixelMathRoundNormalized(tmpDst[1], dstRescale);
        dstPixel[2] = (uint16_t)clPixelMathRoundNormalized(tmpDst[2], dstRescale);
        if (DST_16_HAS_ALPHA()) {
            if (SRC_8_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint16_t)clPixelMathRoundNormalized(tmpDst[3], dstRescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = (uint16_t)dstMaxChannel;
            }
        }
    }
}

static void transformRGB16ToRGB8(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, int srcDepth, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    const int srcMaxChannel = (1 << srcDepth) - 1;
    const float srcRescale = 1.0f / (float)srcMaxChannel;
    static const float dstRescale = 255.0f;

    for (int i = 0; i < pixelCount; ++i) {
        float tmpSrc[4];
        float tmpDst[4];
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        int tmpSrcBytes;
        int tmpDstBytes;

        tmpSrc[0] = (float)srcPixel[0] * srcRescale;
        tmpSrc[1] = (float)srcPixel[1] * srcRescale;
        tmpSrc[2] = (float)srcPixel[2] * srcRescale;
        if (SRC_16_HAS_ALPHA()) {
            tmpSrc[3] = (float)srcPixel[3] * srcRescale;
            tmpSrcBytes = 16;
        } else {
            tmpSrcBytes = 12;
        }
        if (DST_8_HAS_ALPHA()) {
            tmpDstBytes = 16;
        } else {
            tmpDstBytes = 12;
        }

        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)tmpSrc, tmpSrcBytes, (uint8_t *)tmpDst, tmpDstBytes, 1);

        dstPixel[0] = (uint8_t)clPixelMathRoundNormalized(tmpDst[0], dstRescale);
        dstPixel[1] = (uint8_t)clPixelMathRoundNormalized(tmpDst[1], dstRescale);
        dstPixel[2] = (uint8_t)clPixelMathRoundNormalized(tmpDst[2], dstRescale);
        if (DST_8_HAS_ALPHA()) {
            if (SRC_16_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint8_t)clPixelMathRoundNormalized(tmpDst[3], dstRescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 255;
            }
        }
    }
}

static void transformRGB16ToRGB16(struct clContext * C, struct clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, int srcDepth, uint8_t * dstPixels, int dstPixelBytes, int dstDepth, int pixelCount)
{
    const int srcMaxChannel = (1 << srcDepth) - 1;
    const float srcRescale = 1.0f / (float)srcMaxChannel;
    const int dstMaxChannel = (1 << dstDepth) - 1;
    const float dstRescale = (float)dstMaxChannel;

    for (int i = 0; i < pixelCount; ++i) {
        float tmpSrc[4];
        float tmpDst[4];
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        int tmpSrcBytes;
        int tmpDstBytes;

        tmpSrc[0] = (float)srcPixel[0] * srcRescale;
        tmpSrc[1] = (float)srcPixel[1] * srcRescale;
        tmpSrc[2] = (float)srcPixel[2] * srcRescale;
        if (SRC_16_HAS_ALPHA()) {
            tmpSrc[3] = (float)srcPixel[3] * srcRescale;
            tmpSrcBytes = 16;
        } else {
            tmpSrcBytes = 12;
        }
        if (DST_16_HAS_ALPHA()) {
            tmpDstBytes = 16;
        } else {
            tmpDstBytes = 12;
        }

        transformFloatToFloat(C, transform, useCCMM, (uint8_t *)tmpSrc, tmpSrcBytes, (uint8_t *)tmpDst, tmpDstBytes, 1);

        dstPixel[0] = (uint16_t)clPixelMathRoundNormalized(tmpDst[0], dstRescale);
        dstPixel[1] = (uint16_t)clPixelMathRoundNormalized(tmpDst[1], dstRescale);
        dstPixel[2] = (uint16_t)clPixelMathRoundNormalized(tmpDst[2], dstRescale);
        if (DST_16_HAS_ALPHA()) {
            if (SRC_16_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint16_t)clPixelMathRoundNormalized(tmpDst[3], dstRescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = (uint16_t)dstMaxChannel;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Reformatting

static void reformatFloatToFloat(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    COLORIST_UNUSED(C);

    for (int i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        memcpy(dstPixel, srcPixel, sizeof(float) * 3); // all float formats are at least 3 floats
        if (DST_FLOAT_HAS_ALPHA()) {
            if (SRC_FLOAT_HAS_ALPHA()) {
                dstPixel[3] = srcPixel[3];
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 1.0f;
            }
        }
    }
}

static void reformatFloatToRGB8(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    COLORIST_UNUSED(C);

    static const float dstRescale = 255.0f;

    for (int i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint8_t)clPixelMathRoundNormalized(srcPixel[0], dstRescale);
        dstPixel[1] = (uint8_t)clPixelMathRoundNormalized(srcPixel[1], dstRescale);
        dstPixel[2] = (uint8_t)clPixelMathRoundNormalized(srcPixel[2], dstRescale);
        if (DST_8_HAS_ALPHA()) {
            if (SRC_FLOAT_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint8_t)clPixelMathRoundNormalized(srcPixel[3], dstRescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 255;
            }
        }
    }
}

static void reformatFloatToRGB16(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int dstDepth, int pixelCount)
{
    COLORIST_UNUSED(C);

    const int dstMaxChannel = (1 << dstDepth) - 1;
    const float dstRescale = (float)dstMaxChannel;
    for (int i = 0; i < pixelCount; ++i) {
        float * srcPixel = (float *)&srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint16_t)clPixelMathRoundNormalized(srcPixel[0], dstRescale);
        dstPixel[1] = (uint16_t)clPixelMathRoundNormalized(srcPixel[1], dstRescale);
        dstPixel[2] = (uint16_t)clPixelMathRoundNormalized(srcPixel[2], dstRescale);
        if (DST_16_HAS_ALPHA()) {
            if (SRC_FLOAT_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint16_t)clPixelMathRoundNormalized(srcPixel[3], dstRescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = (uint16_t)dstMaxChannel;
            }
        }
    }
}

static void reformatRGB8ToFloat(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    COLORIST_UNUSED(C);

    static const float srcRescale = 1.0f / 255.0f;

    for (int i = 0; i < pixelCount; ++i) {
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (float)srcPixel[0] * srcRescale;
        dstPixel[1] = (float)srcPixel[1] * srcRescale;
        dstPixel[2] = (float)srcPixel[2] * srcRescale;
        if (DST_FLOAT_HAS_ALPHA()) {
            if (SRC_8_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (float)srcPixel[3] * srcRescale;
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 1.0f;
            }
        }
    }
}

static void reformatRGB16ToFloat(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, int srcDepth, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    COLORIST_UNUSED(C);

    const int srcMaxChannel = (1 << srcDepth) - 1;
    const float srcRescale = 1.0f / (float)srcMaxChannel;

    for (int i = 0; i < pixelCount; ++i) {
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        float * dstPixel = (float *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (float)srcPixel[0] * srcRescale;
        dstPixel[1] = (float)srcPixel[1] * srcRescale;
        dstPixel[2] = (float)srcPixel[2] * srcRescale;
        if (DST_FLOAT_HAS_ALPHA()) {
            if (SRC_16_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (float)srcPixel[3] * srcRescale;
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 1.0f;
            }
        }
    }
}

static void reformatRGB8ToRGB8(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    COLORIST_UNUSED(C);

    for (int i = 0; i < pixelCount; ++i) {
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        dstPixel[0] = srcPixel[0];
        dstPixel[1] = srcPixel[1];
        dstPixel[2] = srcPixel[2];
        if (DST_8_HAS_ALPHA()) {
            if (SRC_8_HAS_ALPHA()) {
                dstPixel[3] = srcPixel[3];
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 255;
            }
        }
    }
}

static void reformatRGB16ToRGB16(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, int srcDepth, uint8_t * dstPixels, int dstPixelBytes, int dstDepth, int pixelCount)
{
    COLORIST_UNUSED(C);

    const int srcMaxChannel = (1 << srcDepth) - 1;
    const float srcRescale = 1.0f / (float)srcMaxChannel;
    const int dstMaxChannel = (1 << dstDepth) - 1;
    const float dstRescale = (float)dstMaxChannel;
    const float rescale = srcRescale * dstRescale;

    for (int i = 0; i < pixelCount; ++i) {
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint16_t)((float)srcPixel[0] * rescale);
        dstPixel[1] = (uint16_t)((float)srcPixel[1] * rescale);
        dstPixel[2] = (uint16_t)((float)srcPixel[2] * rescale);
        if (DST_16_HAS_ALPHA()) {
            if (SRC_16_HAS_ALPHA()) {
                dstPixel[3] = (uint16_t)((float)srcPixel[3] * rescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = (uint16_t)dstMaxChannel;
            }
        }
    }
}

static void reformatRGB8ToRGB16(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int dstDepth, int pixelCount)
{
    COLORIST_UNUSED(C);
    const int dstMaxChannel = (1 << dstDepth) - 1;
    const float dstRescale = (float)dstMaxChannel;
    const float rescale = dstRescale / 255.0f;

    for (int i = 0; i < pixelCount; ++i) {
        uint8_t * srcPixel = &srcPixels[i * srcPixelBytes];
        uint16_t * dstPixel = (uint16_t *)&dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint16_t)clPixelMathRoundNormalized((float)srcPixel[0], rescale);
        dstPixel[1] = (uint16_t)clPixelMathRoundNormalized((float)srcPixel[1], rescale);
        dstPixel[2] = (uint16_t)clPixelMathRoundNormalized((float)srcPixel[2], rescale);
        if (DST_16_HAS_ALPHA()) {
            if (SRC_8_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint16_t)clPixelMathRoundNormalized((float)srcPixel[3], rescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = (uint16_t)dstMaxChannel;
            }
        }
    }
}

static void reformatRGB16ToRGB8(struct clContext * C, uint8_t * srcPixels, int srcPixelBytes, int srcDepth, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    COLORIST_UNUSED(C);

    const int srcMaxChannel = (1 << srcDepth) - 1;
    const float rescale = 255.0f / (float)srcMaxChannel;

    for (int i = 0; i < pixelCount; ++i) {
        uint16_t * srcPixel = (uint16_t *)&srcPixels[i * srcPixelBytes];
        uint8_t * dstPixel = &dstPixels[i * dstPixelBytes];
        dstPixel[0] = (uint8_t)clPixelMathRoundNormalized((float)srcPixel[0], rescale);
        dstPixel[1] = (uint8_t)clPixelMathRoundNormalized((float)srcPixel[1], rescale);
        dstPixel[2] = (uint8_t)clPixelMathRoundNormalized((float)srcPixel[2], rescale);
        if (DST_8_HAS_ALPHA()) {
            if (SRC_16_HAS_ALPHA()) {
                // reformat alpha
                dstPixel[3] = (uint8_t)clPixelMathRoundNormalized((float)srcPixel[3], rescale);
            } else {
                // RGB -> RGBA, set full opacity
                dstPixel[3] = 255;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Transform entry point

#define USES_UINT8_T(V) ((V) == 8)
#define USES_UINT16_T(V) (((V) >= 9) && ((V) <= 16))

static void clCCMMTransform(struct clContext * C, struct clTransform * transform, clBool useCCMM, void * srcPixels, void * dstPixels, int pixelCount)
{
    int srcDepth = transform->srcDepth;
    int dstDepth = transform->dstDepth;
    int srcPixelBytes = clTransformFormatToPixelBytes(C, transform->srcFormat, srcDepth);
    int dstPixelBytes = clTransformFormatToPixelBytes(C, transform->dstFormat, dstDepth);

    COLORIST_ASSERT(!transform->srcProfile || transform->srcProfile->ccmm);
    COLORIST_ASSERT(!transform->dstProfile || transform->dstProfile->ccmm);

    // After this point, find a single valid return point from this function, or die

    if (clProfileMatches(C, transform->srcProfile, transform->dstProfile)) {
        // No color conversion necessary, just format conversion

        if (clTransformFormatIsFloat(C, transform->srcFormat, srcDepth) && clTransformFormatIsFloat(C, transform->dstFormat, dstDepth)) {
            // Float to Float, losing or gaining alpha
            reformatFloatToFloat(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
            return;
        }

        if (clTransformFormatIsFloat(C, transform->srcFormat, srcDepth)) {
            // Float -> 8 or 16
            if (USES_UINT8_T(dstDepth)) {
                reformatFloatToRGB8(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT16_T(dstDepth)) {
                reformatFloatToRGB16(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, dstDepth, pixelCount);
                return;
            }
        } else if (clTransformFormatIsFloat(C, transform->dstFormat, dstDepth)) {
            // 8 or 16 -> Float
            if (USES_UINT8_T(srcDepth)) {
                reformatRGB8ToFloat(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT16_T(srcDepth)) {
                reformatRGB16ToFloat(C, srcPixels, srcPixelBytes, srcDepth, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        } else {
            // 8 or 16 -> 8 or 16
            if (USES_UINT8_T(srcDepth) && USES_UINT8_T(dstDepth)) {
                reformatRGB8ToRGB8(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT8_T(srcDepth) && USES_UINT16_T(dstDepth)) {
                reformatRGB8ToRGB16(C, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, dstDepth, pixelCount);
                return;
            }

            if (USES_UINT16_T(srcDepth) && USES_UINT8_T(dstDepth)) {
                reformatRGB16ToRGB8(C, srcPixels, srcPixelBytes, srcDepth, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT16_T(srcDepth) && USES_UINT16_T(dstDepth)) {
                reformatRGB16ToRGB16(C, srcPixels, srcPixelBytes, srcDepth, dstPixels, dstPixelBytes, dstDepth, pixelCount);
                return;
            }
        }
    } else {
        // Color conversion is required

        if (clTransformFormatIsFloat(C, transform->srcFormat, srcDepth) && clTransformFormatIsFloat(C, transform->dstFormat, dstDepth)) {
            // Float to Float
            transformFloatToFloat(C, transform, useCCMM, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
            return;
        }

        if (clTransformFormatIsFloat(C, transform->srcFormat, srcDepth)) {
            // Float -> 8 or 16
            if (USES_UINT8_T(dstDepth)) {
                transformFloatToRGB8(C, transform, useCCMM, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT16_T(dstDepth)) {
                transformFloatToRGB16(C, transform, useCCMM, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, dstDepth, pixelCount);
                return;
            }
        } else if (clTransformFormatIsFloat(C, transform->dstFormat, dstDepth)) {
            // 8 or 16 -> Float
            if (USES_UINT8_T(srcDepth)) {
                transformRGB8ToFloat(C, transform, useCCMM, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT16_T(srcDepth)) {
                transformRGB16ToFloat(C, transform, useCCMM, srcPixels, srcPixelBytes, srcDepth, dstPixels, dstPixelBytes, pixelCount);
                return;
            }
        } else {
            // 8 or 16 -> 8 or 16
            if (USES_UINT8_T(srcDepth) && USES_UINT8_T(dstDepth)) {
                transformRGB8ToRGB8(C, transform, useCCMM, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT8_T(srcDepth) && USES_UINT16_T(dstDepth)) {
                transformRGB8ToRGB16(C, transform, useCCMM, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, dstDepth, pixelCount);
                return;
            }

            if (USES_UINT16_T(srcDepth) && USES_UINT8_T(dstDepth)) {
                transformRGB16ToRGB8(C, transform, useCCMM, srcPixels, srcPixelBytes, srcDepth, dstPixels, dstPixelBytes, pixelCount);
                return;
            }

            if (USES_UINT16_T(srcDepth) && USES_UINT16_T(dstDepth)) {
                transformRGB16ToRGB16(C, transform, useCCMM, srcPixels, srcPixelBytes, srcDepth, dstPixels, dstPixelBytes, dstDepth, pixelCount);
                return;
            }
        }
    }

    COLORIST_FAILURE("clCCMMTransform: Failed to find correct conversion method");
}

// ----------------------------------------------------------------------------
// clTransform API

void clTransformXYZToXYY(struct clContext * C, float * dstXYY, const float * srcXYZ, float whitePointX, float whitePointY)
{
    COLORIST_UNUSED(C);

    float sum = srcXYZ[0] + srcXYZ[1] + srcXYZ[2];
    if (sum <= 0.0f) {
        dstXYY[0] = whitePointX;
        dstXYY[1] = whitePointY;
        dstXYY[2] = 0.0f;
        return;
    }
    dstXYY[0] = srcXYZ[0] / sum;
    dstXYY[1] = srcXYZ[1] / sum;
    dstXYY[2] = srcXYZ[1];
}

void clTransformXYYToXYZ(struct clContext * C, float * dstXYZ, const float * srcXYY)
{
    COLORIST_UNUSED(C);

    if (srcXYY[2] <= 0.0f) {
        dstXYZ[0] = 0.0f;
        dstXYZ[1] = 0.0f;
        dstXYZ[2] = 0.0f;
        return;
    }
    dstXYZ[0] = (srcXYY[0] * srcXYY[2]) / srcXYY[1];
    dstXYZ[1] = srcXYY[2];
    dstXYZ[2] = ((1 - srcXYY[0] - srcXYY[1]) * srcXYY[2]) / srcXYY[1];
}

clTransform * clTransformCreate(struct clContext * C, struct clProfile * srcProfile, clTransformFormat srcFormat, int srcDepth, struct clProfile * dstProfile, clTransformFormat dstFormat, int dstDepth, clTonemap tonemap)
{
    clTransform * transform = clAllocateStruct(clTransform);
    transform->srcProfile = srcProfile;
    transform->dstProfile = dstProfile;
    transform->srcFormat = srcFormat;
    transform->dstFormat = dstFormat;
    transform->srcDepth = srcDepth;
    transform->dstDepth = dstDepth;
    transform->tonemap = tonemap;

    transform->ccmmReady = clFalse;

    transform->lcmsXYZProfile = NULL;
    transform->lcmsSrcToXYZ = NULL;
    transform->lcmsXYZToDst = NULL;
    transform->lcmsCombined = NULL;
    transform->lcmsReady = clFalse;
    return transform;
}

void clTransformDestroy(struct clContext * C, clTransform * transform)
{
    if (transform->lcmsSrcToXYZ) {
        cmsDeleteTransform(transform->lcmsSrcToXYZ);
    }
    if (transform->lcmsXYZToDst) {
        cmsDeleteTransform(transform->lcmsXYZToDst);
    }
    if (transform->lcmsCombined) {
        cmsDeleteTransform(transform->lcmsCombined);
    }
    if (transform->lcmsXYZProfile) {
        cmsCloseProfile(transform->lcmsXYZProfile);
    }
    clFree(transform);
}

static cmsUInt32Number clTransformFormatToLCMSFormat(struct clContext * C, clTransformFormat format)
{
    COLORIST_UNUSED(C);

    switch (format) {
        case CL_XF_XYZ:  return TYPE_XYZ_FLT;
        case CL_XF_RGB:  return TYPE_RGB_FLT;
        case CL_XF_RGBA: return TYPE_RGB_FLT; // CCMM deals with the alpha
    }

    COLORIST_FAILURE("clTransformFormatToLCMSFormat: Unknown transform format");
    return TYPE_RGBA_FLT;
}

clBool clTransformFormatIsFloat(struct clContext * C, clTransformFormat format, int depth)
{
    COLORIST_UNUSED(C);

    switch (format) {
        case CL_XF_XYZ:
            return clTrue;
        case CL_XF_RGB:
        case CL_XF_RGBA:
            return depth == 32;
    }
    return clFalse;
}

int clTransformFormatToPixelBytes(struct clContext * C, clTransformFormat format, int depth)
{
    COLORIST_UNUSED(C);

    switch (format) {
        case CL_XF_XYZ:
            return sizeof(float) * 3;

        case CL_XF_RGB:
            if (depth == 32)
                return sizeof(float) * 3;
            else if (depth > 8)
                return sizeof(uint16_t) * 3;
            else
                return sizeof(uint8_t) * 3;

        case CL_XF_RGBA:
            if (depth == 32)
                return sizeof(float) * 4;
            else if (depth > 8)
                return sizeof(uint16_t) * 4;
            else
                return sizeof(uint8_t) * 4;
    }

    COLORIST_FAILURE("clTransformFormatToPixelBytes: Unknown transform format");
    return sizeof(float) * 4;
}

clBool clTransformUsesCCMM(struct clContext * C, clTransform * transform)
{
    clBool useCCMM = C->ccmmAllowed;
    if (!clProfileUsesCCMM(C, transform->srcProfile)) {
        useCCMM = clFalse;
    }
    if (!clProfileUsesCCMM(C, transform->dstProfile)) {
        useCCMM = clFalse;
    }
    return useCCMM;
}

const char * clTransformCMMName(struct clContext * C, clTransform * transform)
{
    return clTransformUsesCCMM(C, transform) ? "CCMM" : "LCMS";
}

float clTransformGetLuminanceScale(struct clContext * C, clTransform * transform)
{
    clTransformPrepare(C, transform);
    return transform->srcLuminanceScale / transform->dstLuminanceScale * transform->srcCurveScale / transform->dstCurveScale;
}

typedef struct clTransformTask
{
    clContext * C;
    clTransform * transform;
    void * inPixels;
    void * outPixels;
    int pixelCount;
    clBool useCCMM;
} clTransformTask;

static void transformTaskFunc(clTransformTask * info)
{
    clCCMMTransform(info->C, info->transform, info->useCCMM, info->inPixels, info->outPixels, info->pixelCount);
}

void clTransformRun(struct clContext * C, clTransform * transform, int taskCount, void * srcPixels, void * dstPixels, int pixelCount)
{
    int srcPixelBytes = clTransformFormatToPixelBytes(C, transform->srcFormat, transform->srcDepth);
    int dstPixelBytes = clTransformFormatToPixelBytes(C, transform->dstFormat, transform->dstDepth);
    clBool useCCMM = clTransformUsesCCMM(C, transform);

    clTransformPrepare(C, transform);

    if (taskCount > pixelCount) {
        // This is a dumb corner case I'm not too worried about.
        taskCount = pixelCount;
    }

    if (taskCount > 1) {
        clContextLog(C, "convert", 1, "Using %d threads to pixel transform.", taskCount);
    }

    if (taskCount == 1) {
        // Don't bother making any new threads
        clTransformTask info;
        info.C = C;
        info.transform = transform;
        info.inPixels = srcPixels;
        info.outPixels = dstPixels;
        info.pixelCount = pixelCount;
        info.useCCMM = useCCMM;
        transformTaskFunc(&info);
    } else {
        uint8_t * uSrcPixels = (uint8_t *)srcPixels;
        uint8_t * uDstPixels = (uint8_t *)dstPixels;
        int pixelsPerTask = pixelCount / taskCount;
        int lastTaskPixelCount = pixelCount - (pixelsPerTask * (taskCount - 1));
        clTask ** tasks;
        clTransformTask * infos;
        int i;

        tasks = clAllocate(taskCount * sizeof(clTask *));
        infos = clAllocate(taskCount * sizeof(clTransformTask));
        for (i = 0; i < taskCount; ++i) {
            infos[i].C = C;
            infos[i].transform = transform;
            infos[i].inPixels = &uSrcPixels[i * pixelsPerTask * srcPixelBytes];
            infos[i].outPixels = &uDstPixels[i * pixelsPerTask * dstPixelBytes];
            infos[i].pixelCount = (i == (taskCount - 1)) ? lastTaskPixelCount : pixelsPerTask;
            infos[i].useCCMM = useCCMM;
            tasks[i] = clTaskCreate(C, (clTaskFunc)transformTaskFunc, &infos[i]);
        }

        for (i = 0; i < taskCount; ++i) {
            clTaskDestroy(C, tasks[i]);
        }

        clFree(tasks);
        clFree(infos);
    }
}
