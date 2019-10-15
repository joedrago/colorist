#include "colorist/transform.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/task.h"

#include "gb_math.h"

#include <math.h>
#include <string.h>

// The small amount after the 1.0 here buys us  a little imprecision wiggle
// room on an automatic tonemap. (It's ok to clip if our luminance scale is
// this close.)
#define AUTO_TONEMAP_LUMINANCE_SCALE_THRESHOLD (1.001f)

#define SRC_FLOAT_HAS_ALPHA() (srcChannelCount > 3)
#define DST_FLOAT_HAS_ALPHA() (dstChannelCount > 3)

static cmsUInt32Number clTransformFormatToLCMSFormat(struct clContext * C, clTransformFormat format);
static int clTransformFormatToChannelCount(struct clContext * C, clTransformFormat format);

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

static const float HLG_A = 0.17883277f;
static const float HLG_B = 0.28466892f;    // 1.0f - (4.0f * HLG_A);
static const float HLG_C = 0.55991072953f; // 0.5f - HLG_A * logf(4.0f * HLG_A);
static const float HLG_ONE_TWELFTH = 1.0f / 12.0f;

static float HLG_EOTF(float N, float maxLuminance)
{
    float L;
    if (N < 0.5f) {
        L = (N * N) / 3.0f;
    } else {
        L = (expf((N - HLG_C) / HLG_A) + HLG_B) / 12.0f;
    }

    // This includes the HLG OOTF here
    float exponent = 1.2f + (0.42f * log10f(maxLuminance / 1000.0f));
    return powf(L, exponent);
}

static float HLG_OETF(float L, float maxLuminance)
{
    // This includes the HLG OOTF here
    float exponent = 1.2f + (0.42f * log10f(maxLuminance / 1000.0f));
    float N = powf(L, 1.0f / exponent);

    if (N <= HLG_ONE_TWELFTH) {
        return sqrtf(3.0f * N);
    }
    return HLG_A * logf((12.0f * N) - HLG_B) + HLG_C;
}

static float hlgDiffuseWhite(float peakWhite)
{
    float base = (expf((0.75f - HLG_C) / HLG_A) + HLG_B) / 12.0f;
    float exponent = 1.2f + (0.42f * log10f(peakWhite / 1000.0f));
    return peakWhite * powf(base, exponent);
}

// Find the next integral HLG peak white, given a goal diffuse white
int clTransformCalcHLGLuminance(int diffuseWhite)
{
    float goalDiffuseWhite = (float)diffuseWhite;
    int L = 1;
    int R = 100000;
    while (L < R) {
        int M = (L + R) >> 1;
        float attempt = hlgDiffuseWhite((float)M);
        if (attempt <= goalDiffuseWhite) {
            L = M + 1;
        } else {
            R = M;
        }
    }
    return L;
}

int clTransformCalcDefaultLuminanceFromHLG(int hlgLuminance)
{
    int lum = (int)clPixelMathRoundf(hlgDiffuseWhite((float)hlgLuminance));
    return lum;
}

// From http://docs-hoffmann.de/ciexyz29082000.pdf, Section 11.4
void clTransformDeriveXYZMatrix(struct clContext * C, clProfilePrimaries * primaries, gbMat3 * toXYZ)
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

static clBool derivePrimariesAndXTF(struct clContext * C,
                                    struct clProfile * profile,
                                    clProfilePrimaries * outPrimaries,
                                    clTransformTransferFunction * outXTF,
                                    float * outGamma)
{
    if (profile) {
        clProfileCurve curve;
        int luminance = 0;

        if (clProfileQuery(C, profile, outPrimaries, &curve, &luminance)) {
            if (curve.type == CL_PCT_HLG) {
                *outXTF = CL_XTF_HLG;
                *outGamma = 0.0f;
            } else if (curve.type == CL_PCT_PQ) {
                *outXTF = CL_XTF_PQ;
                *outGamma = 0.0f;
            } else {
                *outXTF = CL_XTF_GAMMA;
                *outGamma = curve.gamma;
            }
        } else {
            clContextLogError(C, "clTransformDeriveXYZMatrix: fatal error querying profile");
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

        clBool srcUsesHLGScaling = clFalse;
        if (transform->srcProfile) {
            clProfilePrimaries srcPrimaries;
            clProfileCurve srcCurve;
            int srcLuminance;

            clProfileQuery(C, transform->srcProfile, &srcPrimaries, &srcCurve, &srcLuminance);
            if (srcLuminance == CL_LUMINANCE_UNSPECIFIED) {
                srcLuminance = C->defaultLuminance;
                if (srcCurve.type == CL_PCT_HLG) {
                    srcUsesHLGScaling = clTrue;
                }
            }
            transform->srcLuminanceScale = (float)srcLuminance;
            transform->srcCurveScale = srcCurve.implicitScale;
            transform->whitePointX = srcPrimaries.white[0];
            transform->whitePointY = srcPrimaries.white[1];
        } else {
            transform->srcLuminanceScale = 1.0f;
            transform->srcCurveScale = 1.0f;
        }

        clBool dstUsesHLGScaling = clFalse;
        if (transform->dstProfile) {
            clProfilePrimaries dstPrimaries;
            clProfileCurve dstCurve;
            int dstLuminance;

            clProfileQuery(C, transform->dstProfile, &dstPrimaries, &dstCurve, &dstLuminance);
            if (dstLuminance == CL_LUMINANCE_UNSPECIFIED) {
                dstLuminance = C->defaultLuminance;
                if (dstCurve.type == CL_PCT_HLG) {
                    dstUsesHLGScaling = clTrue;
                }
            }
            transform->dstLuminanceScale = (float)dstLuminance;
            transform->dstCurveScale = dstCurve.implicitScale;
            transform->whitePointX = dstPrimaries.white[0];
            transform->whitePointY = dstPrimaries.white[1];
        } else {
            transform->dstLuminanceScale = 1.0f;
            transform->dstCurveScale = 1.0f;
        }

        if (srcUsesHLGScaling || dstUsesHLGScaling) {
            transform->ccmmHLGLuminance = (float)clTransformCalcHLGLuminance(C->defaultLuminance);
            clContextLog(C,
                         "hlg",
                         1,
                         "HLG: Max Luminance %2.2f nits, based on diffuse white of %d nits (--deflum)",
                         transform->ccmmHLGLuminance,
                         C->defaultLuminance);
            if (srcUsesHLGScaling) {
                transform->srcLuminanceScale = transform->ccmmHLGLuminance;
            }
            if (dstUsesHLGScaling) {
                transform->dstLuminanceScale = transform->ccmmHLGLuminance;
            }
        }

        switch (transform->tonemap) {
            case CL_TONEMAP_AUTO:
                transform->tonemapEnabled =
                    (((transform->srcLuminanceScale * transform->srcCurveScale) /
                      (transform->dstLuminanceScale * transform->dstCurveScale)) > AUTO_TONEMAP_LUMINANCE_SCALE_THRESHOLD)
                        ? clTrue
                        : clFalse;
                break;
            case CL_TONEMAP_ON:
                transform->tonemapEnabled = clTrue;
                break;
            case CL_TONEMAP_OFF:
                transform->tonemapEnabled = clFalse;
                break;
        }

        if (!useCCMM || !transform->srcProfile || !transform->dstProfile || transform->tonemapEnabled ||
            (fabsf((transform->srcLuminanceScale * transform->srcCurveScale) -
                   (transform->dstLuminanceScale * transform->dstCurveScale)) > 0.00001f)) {
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
                clTransformDeriveXYZMatrix(C, &srcPrimaries, &transform->ccmmSrcToXYZ);
            } else {
                gb_mat3_identity(&transform->ccmmSrcToXYZ);
            }
            if (transform->dstProfile) {
                clTransformDeriveXYZMatrix(C, &dstPrimaries, &dstToXYZ);
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
                                                            srcProfileHandle,
                                                            srcFormat,
                                                            transform->lcmsXYZProfile,
                                                            TYPE_XYZ_FLT,
                                                            INTENT_ABSOLUTE_COLORIMETRIC,
                                                            cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

            transform->lcmsXYZToDst = cmsCreateTransformTHR(C->lcms,
                                                            transform->lcmsXYZProfile,
                                                            TYPE_XYZ_FLT,
                                                            dstProfileHandle,
                                                            dstFormat,
                                                            INTENT_ABSOLUTE_COLORIMETRIC,
                                                            cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

            transform->lcmsCombined = cmsCreateTransformTHR(C->lcms,
                                                            srcProfileHandle,
                                                            srcFormat,
                                                            dstProfileHandle,
                                                            dstFormat,
                                                            INTENT_ABSOLUTE_COLORIMETRIC,
                                                            cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

            transform->lcmsReady = clTrue;
        }
    }
}

// The real color conversion function
static void colorConvert(struct clContext * C,
                         struct clTransform * transform,
                         clBool useCCMM,
                         float * srcPixels,
                         int srcChannelCount,
                         float * dstPixels,
                         int dstChannelCount,
                         int pixelCount)
{
    for (int i = 0; i < pixelCount; ++i) {
        float * srcPixel = &srcPixels[i * srcChannelCount];
        float * dstPixel = &dstPixels[i * dstChannelCount];
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
                case CL_XTF_HLG:
                    src.x = HLG_EOTF((srcPixel[0] >= 0.0f) ? srcPixel[0] : 0.0f, transform->ccmmHLGLuminance);
                    src.y = HLG_EOTF((srcPixel[1] >= 0.0f) ? srcPixel[1] : 0.0f, transform->ccmmHLGLuminance);
                    src.z = HLG_EOTF((srcPixel[2] >= 0.0f) ? srcPixel[2] : 0.0f, transform->ccmmHLGLuminance);
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
                case CL_XTF_HLG:
                    gb_mat3_mul_vec3((gbVec3 *)tmp, &transform->ccmmXYZToDst, src);
                    if (transform->dstProfile) {               // don't clamp XYZ
                        tmp[0] = CL_CLAMP(tmp[0], 0.0f, 1.0f); // clamp
                        tmp[1] = CL_CLAMP(tmp[1], 0.0f, 1.0f); // clamp
                        tmp[2] = CL_CLAMP(tmp[2], 0.0f, 1.0f); // clamp
                    }
                    dstPixel[0] = HLG_OETF((tmp[0] >= 0.0f) ? tmp[0] : 0.0f, transform->ccmmHLGLuminance);
                    dstPixel[1] = HLG_OETF((tmp[1] >= 0.0f) ? tmp[1] : 0.0f, transform->ccmmHLGLuminance);
                    dstPixel[2] = HLG_OETF((tmp[2] >= 0.0f) ? tmp[2] : 0.0f, transform->ccmmHLGLuminance);
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
// Transform entry point

static void clCCMMTransform(struct clContext * C, struct clTransform * transform, clBool useCCMM, float * srcPixels, float * dstPixels, int pixelCount)
{
    int srcChannelCount = clTransformFormatToChannelCount(C, transform->srcFormat);
    int dstChannelCount = clTransformFormatToChannelCount(C, transform->dstFormat);

    // COLORIST_ASSERT(!transform->srcProfile || transform->srcProfile->ccmm);
    // COLORIST_ASSERT(!transform->dstProfile || transform->dstProfile->ccmm);

    if (clProfileMatches(C, transform->srcProfile, transform->dstProfile)) {
        // No color conversion necessary, just repack honoring src/dst alpha

        for (int i = 0; i < pixelCount; ++i) {
            float * srcPixel = &srcPixels[i * srcChannelCount];
            float * dstPixel = &dstPixels[i * dstChannelCount];
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
    } else {
        // Color conversion is required

        colorConvert(C, transform, useCCMM, srcPixels, srcChannelCount, dstPixels, dstChannelCount, pixelCount);
    }
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

float clTransformCalcMaxY(clContext * C, clTransform * linearFromXYZ, clTransform * linearToXYZ, float x, float y)
{
    float floatXYZ[3];
    float floatRGB[3];
    float maxChannel;
    cmsCIEXYZ XYZ;
    cmsCIExyY xyY;
    xyY.x = x;
    xyY.y = y;
    xyY.Y = 1.0f; // start with max luminance
    cmsxyY2XYZ(&XYZ, &xyY);
    floatXYZ[0] = (float)XYZ.X;
    floatXYZ[1] = (float)XYZ.Y;
    floatXYZ[2] = (float)XYZ.Z;
    clTransformRun(C, linearFromXYZ, floatXYZ, floatRGB, 1);
    maxChannel = floatRGB[0];
    if (maxChannel < floatRGB[1])
        maxChannel = floatRGB[1];
    if (maxChannel < floatRGB[2])
        maxChannel = floatRGB[2];
    floatRGB[0] /= maxChannel;
    floatRGB[1] /= maxChannel;
    floatRGB[2] /= maxChannel;
    clTransformRun(C, linearToXYZ, floatRGB, floatXYZ, 1);
    return floatXYZ[1];
}

clTransform * clTransformCreate(struct clContext * C,
                                struct clProfile * srcProfile,
                                clTransformFormat srcFormat,
                                struct clProfile * dstProfile,
                                clTransformFormat dstFormat,
                                clTonemap tonemap)
{
    clTransform * transform = clAllocateStruct(clTransform);
    transform->srcProfile = srcProfile;
    transform->dstProfile = dstProfile;
    transform->srcFormat = srcFormat;
    transform->dstFormat = dstFormat;
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
        case CL_XF_XYZ:
            return TYPE_XYZ_FLT;
        case CL_XF_RGB:
            return TYPE_RGB_FLT;
        case CL_XF_RGBA:
            return TYPE_RGB_FLT; // CCMM deals with the alpha
    }

    COLORIST_FAILURE("clTransformFormatToLCMSFormat: Unknown transform format");
    return TYPE_RGBA_FLT;
}

static int clTransformFormatToChannelCount(struct clContext * C, clTransformFormat format)
{
    COLORIST_UNUSED(C);

    switch (format) {
        case CL_XF_XYZ:
        case CL_XF_RGB:
            return 3;

        case CL_XF_RGBA:
            return 4;
    }

    COLORIST_FAILURE("clTransformFormatToChannelCount: Unknown transform format");
    return 4;
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
    float * inPixels;
    float * outPixels;
    int pixelCount;
    clBool useCCMM;
} clTransformTask;

static void transformTaskFunc(clTransformTask * info)
{
    clCCMMTransform(info->C, info->transform, info->useCCMM, info->inPixels, info->outPixels, info->pixelCount);
}

void clTransformRun(struct clContext * C, clTransform * transform, float * srcPixels, float * dstPixels, int pixelCount)
{
    int srcChannelCount = clTransformFormatToChannelCount(C, transform->srcFormat);
    int dstChannelCount = clTransformFormatToChannelCount(C, transform->dstFormat);
    clBool useCCMM = clTransformUsesCCMM(C, transform);
    int taskCount = C->jobs;

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
            infos[i].inPixels = &srcPixels[i * pixelsPerTask * srcChannelCount];
            infos[i].outPixels = &dstPixels[i * pixelsPerTask * dstChannelCount];
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
