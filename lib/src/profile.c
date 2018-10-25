// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/profile.h"

#include "colorist/context.h"
#include "colorist/raw.h"

#include "lcms2_plugin.h"
#include "md5.h"

#include <math.h>
#include <string.h>

// from cmsio1.c
extern cmsBool _cmsReadCHAD(cmsMAT3 * Dest, cmsHPROFILE hProfile);

clProfile * clProfileCreateStock(struct clContext * C, clProfileStock stock)
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;
    const char * description;
    COLORIST_ASSERT((stock >= 0) && (stock < CL_PS_COUNT));
    switch (stock) {
        case CL_PS_SRGB:
        default:
            primaries.red[0] = 0.64f;
            primaries.red[1] = 0.33f;
            primaries.green[0] = 0.30f;
            primaries.green[1] = 0.60f;
            primaries.blue[0] = 0.15f;
            primaries.blue[1] = 0.06f;
            primaries.white[0] = 0.3127f;
            primaries.white[1] = 0.3290f;
            curve.type = CL_PCT_GAMMA;
            curve.gamma = COLORIST_SRGB_GAMMA;
            maxLuminance = COLORIST_DEFAULT_LUMINANCE;
            description = "Colorist SRGB";
            break;
    }
    return clProfileCreate(C, &primaries, &curve, maxLuminance, description);
}

clProfile * clProfileClone(struct clContext * C, clProfile * profile)
{
    clRaw packed;
    clProfile * clone;
    memset(&packed, 0, sizeof(packed));
    if (!clProfilePack(C, profile, &packed)) {
        return NULL;
    }

    clone = clProfileParse(C, packed.ptr, packed.size, profile->description);
    clRawFree(C, &packed);
    return clone;
}

clProfile * clProfileParse(struct clContext * C, const uint8_t * icc, int iccLen, const char * description)
{
    clProfile * profile = clAllocateStruct(clProfile);
    profile->handle = cmsOpenProfileFromMemTHR(C->lcms, icc, iccLen);
    if (!profile->handle) {
        clFree(profile);
        return NULL;
    }
    if (description) {
        profile->description = clContextStrdup(C, description);
    } else {
        char * embeddedDescription = clProfileGetMLU(C, profile, "desc", "en", "US");
        COLORIST_ASSERT(!profile->description);
        if (embeddedDescription) {
            profile->description = embeddedDescription; // take ownership
        } else {
            profile->description = clContextStrdup(C, "Unknown");
        }
    }

    // Save copy of packed data to keep a byte-for-byte payload unless the profile is modified
    clRawSet(C, &profile->raw, icc, iccLen);

    // Calculate signature
    {
        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, icc, iccLen);
        MD5_Final(profile->signature, &ctx);
    }

    // See if colorist CMM can handle this profile
    {
        clProfilePrimaries primaries = { 0 };
        clProfileCurve curve = { 0 };
        int luminance = 0;
        profile->ccmm = clFalse; // Start with unfriendly
        if (clProfileHasPQSignature(C, profile, NULL)) {
            // CCMM specifically supports any special profiles recognized as PQ
            profile->ccmm = clTrue;
        } else if (clProfileQuery(C, profile, &primaries, &curve, &luminance)) {
            // TODO: Be way more restrictive here
            if (curve.type == CL_PCT_GAMMA) {
                profile->ccmm = clTrue;
            }
        }
    }
    return profile;
}

clProfile * clProfileCreate(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance, const char * description)
{
    clProfile * profile = clAllocateStruct(clProfile);
    cmsToneCurve * curves[3];
    cmsCIExyYTRIPLE dstPrimaries;
    cmsCIExyY dstWhitePoint;
    cmsCIEXYZ lumi;

    dstPrimaries.Red.x = primaries->red[0];
    dstPrimaries.Red.y = primaries->red[1];
    dstPrimaries.Red.Y = 0.0f; // unused
    dstPrimaries.Green.x = primaries->green[0];
    dstPrimaries.Green.y = primaries->green[1];
    dstPrimaries.Green.Y = 0.0f; // unused
    dstPrimaries.Blue.x = primaries->blue[0];
    dstPrimaries.Blue.y = primaries->blue[1];
    dstPrimaries.Blue.Y = 0.0f; // unused
    dstWhitePoint.x = primaries->white[0];
    dstWhitePoint.y = primaries->white[1];
    dstWhitePoint.Y = 1.0f;

    curves[0] = cmsBuildGamma(C->lcms, curve->gamma);
    curves[1] = curves[0];
    curves[2] = curves[0];
    profile->handle = cmsCreateRGBProfileTHR(C->lcms, &dstWhitePoint, &dstPrimaries, curves);
    cmsFreeToneCurve(curves[0]);
    if (!profile->handle) {
        clFree(profile);
        return NULL;
    }

    lumi.X = 0.0f;
    lumi.Y = (cmsFloat64Number)maxLuminance;
    lumi.Z = 0.0f;
    cmsWriteTag(profile->handle, cmsSigLuminanceTag, &lumi);

    profile->description = description ? strdup(description) : NULL;
    if (profile->description) {
        clProfileSetMLU(C, profile, "desc", "en", "US", profile->description);
    }
    memset(&profile->raw, 0, sizeof(profile->raw));
    clProfileReload(C, profile);
    return profile;
}

clBool clProfilePack(struct clContext * C, clProfile * profile, clRaw * out)
{
    if (profile->raw.size > 0) {
        clRawClone(C, out, &profile->raw);
    } else {
        cmsUInt32Number bytesNeeded;
        if (!cmsSaveProfileToMem(profile->handle, NULL, &bytesNeeded)) {
            return clFalse;
        }
        clRawRealloc(C, out, bytesNeeded);
        if (!cmsSaveProfileToMem(profile->handle, out->ptr, &bytesNeeded)) {
            clRawFree(C, out);
            return clFalse;
        }
    }
    return clTrue;
}

int clProfileSize(struct clContext * C, clProfile * profile)
{
    int ret;
    if (profile->raw.size > 0) {
        ret = profile->raw.size;
    } else {
        clRaw raw;
        memset(&raw, 0, sizeof(raw));
        if (!clProfilePack(C, profile, &raw)) {
            return 0;
        }
        ret = raw.size;
        clRawFree(C, &raw);
    }
    return ret;
}

clProfile * clProfileRead(struct clContext * C, const char * filename)
{
    clProfile * profile;
    clRaw rawProfile;
    int bytes;
    FILE * f;
    f = fopen(filename, "rb");
    if (!f) {
        clContextLogError(C, "Can't open ICC file for read: %s", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    memset(&rawProfile, 0, sizeof(rawProfile));
    clRawRealloc(C, &rawProfile, bytes);
    fread(rawProfile.ptr, bytes, 1, f);
    fclose(f);
    profile = clProfileParse(C, rawProfile.ptr, rawProfile.size, NULL);
    clRawFree(C, &rawProfile);
    return profile;
}

clBool clProfileWrite(struct clContext * C, clProfile * profile, const char * filename)
{
    clRaw rawProfile;
    FILE * f;
    int itemsWritten;

    f = fopen(filename, "wb");
    if (!f) {
        clContextLogError(C, "Can't open file for write: %s", filename);
        return 1;
    }
    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, profile, &rawProfile)) {
        clContextLogError(C, "Can't pack ICC profile");
        fclose(f);
        return clFalse;
    }
    itemsWritten = fwrite(rawProfile.ptr, rawProfile.size, 1, f);
    if (itemsWritten != 1) {
        clContextLogError(C, "Failed to write ICC profile");
        fclose(f);
        return clFalse;
    }
    fclose(f);
    clRawFree(C, &rawProfile);
    return clTrue;
}

clBool clProfileReload(struct clContext * C, clProfile * profile)
{
    clProfile * tmpProfile;
    clRaw raw;
    memset(&raw, 0, sizeof(raw));
    if (!clProfilePack(C, profile, &raw)) {
        return clFalse;
    }
    tmpProfile = clProfileParse(C, raw.ptr, raw.size, profile->description);
    clRawFree(C, &raw);
    if (!tmpProfile) {
        return clFalse;
    }

    // swap contents (yuck!)
    {
        clProfile t;
        memcpy(&t, profile, sizeof(clProfile));
        memcpy(profile, tmpProfile, sizeof(clProfile));
        memcpy(tmpProfile, &t, sizeof(clProfile));
    }
    clProfileDestroy(C, tmpProfile);
    return clTrue;
}

void clProfileDestroy(struct clContext * C, clProfile * profile)
{
    clFree(profile->description);
    cmsCloseProfile(profile->handle);
    clRawFree(C, &profile->raw);
    clFree(profile);
}

clBool clProfileQuery(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries, clProfileCurve * curve, int * luminance)
{
    if (primaries) {
        cmsMAT3 chad;
        cmsMAT3 invChad;
        cmsMAT3 tmpColorants;
        cmsMAT3 colorants;
        cmsCIEXYZ src;
        cmsCIExyY dst;
        cmsCIEXYZ adaptedWhiteXYZ;
        cmsCIEXYZ * redXYZ   = (cmsCIEXYZ *)cmsReadTag(profile->handle, cmsSigRedColorantTag);
        cmsCIEXYZ * greenXYZ = (cmsCIEXYZ *)cmsReadTag(profile->handle, cmsSigGreenColorantTag);
        cmsCIEXYZ * blueXYZ  = (cmsCIEXYZ *)cmsReadTag(profile->handle, cmsSigBlueColorantTag);
        cmsCIEXYZ * whiteXYZ = (cmsCIEXYZ *)cmsReadTag(profile->handle, cmsSigMediaWhitePointTag);
        if (whiteXYZ == NULL)
            return clFalse;

        if ((redXYZ == NULL) || (greenXYZ == NULL) || (blueXYZ == NULL)) {
            // No colorant tags. See if we can harvest them (poorly) from the A2B0 tag. (yuck)
            cmsUInt32Number aToBTagSize = cmsReadRawTag(profile->handle, cmsSigAToB0Tag, NULL, 0);
            if (aToBTagSize >= 32) { // A2B0 tag is present. Allow it to override primaries and tone curves.
                int i;
                float matrix[9];
                uint32_t matrixOffset = 0;
                uint8_t * rawA2B0 = clAllocate(aToBTagSize);
                cmsReadRawTag(profile->handle, cmsSigAToB0Tag, rawA2B0, aToBTagSize);

                memcpy(&matrixOffset, rawA2B0 + 16, sizeof(matrixOffset));
                matrixOffset = clNTOHL(matrixOffset);
                if (matrixOffset == 0) {
                    // No matrix present
                    clFree(rawA2B0);
                    return clFalse;
                }
                if ((matrixOffset + 18) > aToBTagSize) {
                    // No room to read matrix
                    clFree(rawA2B0);
                    return clFalse;
                }

                for (i = 0; i < 9; ++i) {
                    cmsS15Fixed16Number e;
                    memcpy(&e, &rawA2B0[matrixOffset + (i * 4)], 4);
                    matrix[i] = (float)_cms15Fixed16toDouble(clNTOHL(e));
                }
                _cmsVEC3init(&tmpColorants.v[0], matrix[0], matrix[1], matrix[2]);
                _cmsVEC3init(&tmpColorants.v[1], matrix[3], matrix[4], matrix[5]);
                _cmsVEC3init(&tmpColorants.v[2], matrix[6], matrix[7], matrix[8]);
                clFree(rawA2B0);
            }
        } else {
            // Found rXYZ, gXYZ, bXYZ. Pull out the colorants from them.
            _cmsVEC3init(&tmpColorants.v[0], redXYZ->X, greenXYZ->X, blueXYZ->X);
            _cmsVEC3init(&tmpColorants.v[1], redXYZ->Y, greenXYZ->Y, blueXYZ->Y);
            _cmsVEC3init(&tmpColorants.v[2], redXYZ->Z, greenXYZ->Z, blueXYZ->Z);
        }

        if (_cmsReadCHAD(&chad, profile->handle) && _cmsMAT3inverse(&chad, &invChad)) {
            // Always adapt the colorants with the chad tag (if wtpt is D50, it'll be identity)
            _cmsMAT3per(&colorants, &invChad, &tmpColorants);

            if ((cmsGetEncodedICCversion(profile->handle) >= 0x4000000) && cmsIsTag(profile->handle, cmsSigChromaticAdaptationTag)) {
                // Newer version with a chad tag set, adapt white point
                cmsVEC3 srcWP, dstWP;
                cmsCIExyY whiteXYY;
                cmsXYZ2xyY(&whiteXYY, whiteXYZ);
                srcWP.n[VX] = whiteXYZ->X;
                srcWP.n[VY] = whiteXYZ->Y;
                srcWP.n[VZ] = whiteXYZ->Z;
                _cmsMAT3eval(&dstWP, &invChad, &srcWP);
                adaptedWhiteXYZ.X = dstWP.n[VX];
                adaptedWhiteXYZ.Y = dstWP.n[VY];
                adaptedWhiteXYZ.Z = dstWP.n[VZ];
            } else {
                // Old version, or new version without a chad tag, leave wtpt alone
                adaptedWhiteXYZ = *whiteXYZ;
            }
        } else {
            colorants = tmpColorants;
            adaptedWhiteXYZ = *whiteXYZ;
        }

        src.X = colorants.v[0].n[VX];
        src.Y = colorants.v[1].n[VX];
        src.Z = colorants.v[2].n[VX];
        cmsXYZ2xyY(&dst, &src);
        primaries->red[0] = (float)dst.x;
        primaries->red[1] = (float)dst.y;
        src.X = colorants.v[0].n[VY];
        src.Y = colorants.v[1].n[VY];
        src.Z = colorants.v[2].n[VY];
        cmsXYZ2xyY(&dst, &src);
        primaries->green[0] = (float)dst.x;
        primaries->green[1] = (float)dst.y;
        src.X = colorants.v[0].n[VZ];
        src.Y = colorants.v[1].n[VZ];
        src.Z = colorants.v[2].n[VZ];
        cmsXYZ2xyY(&dst, &src);
        primaries->blue[0] = (float)dst.x;
        primaries->blue[1] = (float)dst.y;
        cmsXYZ2xyY(&dst, &adaptedWhiteXYZ);
        primaries->white[0] = (float)dst.x;
        primaries->white[1] = (float)dst.y;
    }

    if (curve) {
        cmsToneCurve * toneCurve = (cmsToneCurve *)cmsReadTag(profile->handle, cmsSigRedTRCTag);
        if (toneCurve) {
            int curveType = cmsGetToneCurveParametricType(toneCurve);
            float gamma = (float)cmsEstimateGamma(toneCurve, 1.0f);
            curve->type = (curveType == 1) ? CL_PCT_GAMMA : CL_PCT_COMPLEX;
            curve->gamma = gamma;
        } else {
            if (cmsReadRawTag(profile->handle, cmsSigAToB0Tag, NULL, 0) > 0) {
                curve->type = CL_PCT_COMPLEX;
                curve->gamma = -1.0f;
            } else {
                curve->type = CL_PCT_UNKNOWN;
                curve->gamma = 0.0f;
            }
        }

        // Check for A2B0 implicit scale in the matrix curve, for reporting purposes
        curve->matrixCurveScale = 0.0f;
        {
            cmsUInt32Number aToBTagSize = cmsReadRawTag(profile->handle, cmsSigAToB0Tag, NULL, 0);
            if (aToBTagSize >= 32) { // A2B0 tag is present. Check for a matrix scale on para curve types 1 and above
                uint8_t * rawA2B0 = clAllocate(aToBTagSize);
                uint32_t matrixCurveOffset = 0;

                cmsReadRawTag(profile->handle, cmsSigAToB0Tag, rawA2B0, aToBTagSize);
                memcpy(&matrixCurveOffset, rawA2B0 + 20, sizeof(matrixCurveOffset));
                matrixCurveOffset = clNTOHL(matrixCurveOffset);
                if (matrixCurveOffset == 0) {
                    // No matrix curve present
                    clFree(rawA2B0);
                    return clFalse;
                }

                if (!memcmp(&rawA2B0[matrixCurveOffset], "para", 4)) {
                    uint16_t curveType;
                    memcpy(&curveType, &rawA2B0[matrixCurveOffset + 8], 2);
                    curveType = clNTOHS(curveType);
                    if ((curveType > 0) && (curveType <= 4)) {
                        // Guaranteed to have a g(0) argument and an a(1) argument. a^g is the scale.
                        float g, a;
                        cmsS15Fixed16Number e;
                        memcpy(&e, &rawA2B0[matrixCurveOffset + 12], 4);
                        g = (float)_cms15Fixed16toDouble(clNTOHL(e));
                        memcpy(&e, &rawA2B0[matrixCurveOffset + 16], 4);
                        a = (float)_cms15Fixed16toDouble(clNTOHL(e));
                        curve->matrixCurveScale = powf(a, g);
                    }
                }
                clFree(rawA2B0);
            }
        }
    }

    if (luminance) {
        cmsCIEXYZ * lumi = (cmsCIEXYZ *)cmsReadTag(profile->handle, cmsSigLuminanceTag);
        if (lumi) {
            *luminance = (int)lumi->Y;
        } else {
            *luminance = 0;
        }
    }
    return clTrue;
}

char * clProfileGetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3])
{
    cmsTagSignature tagSignature;
    cmsUInt32Number bytes;
    cmsMLU * mlu;
    char * ascii;
    uint8_t * rawTagPtr = (uint8_t *)&tagSignature;
    rawTagPtr[0] = tag[3];
    rawTagPtr[1] = tag[2];
    rawTagPtr[2] = tag[1];
    rawTagPtr[3] = tag[0];
    mlu = cmsReadTag(profile->handle, tagSignature);
    if (!mlu) {
        return NULL;
    }
    bytes = cmsMLUgetASCII(mlu, languageCode, countryCode, NULL, 0);
    if (!bytes) {
        return NULL;
    }
    ascii = clAllocate(bytes);
    cmsMLUgetASCII(mlu, languageCode, countryCode, ascii, bytes);
    return ascii;
}

clBool clProfileSetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3], const char * ascii)
{
    cmsTagSignature tagSignature;
    cmsMLU * mlu;
    uint8_t * rawTagPtr = (uint8_t *)&tagSignature;
    rawTagPtr[0] = tag[3];
    rawTagPtr[1] = tag[2];
    rawTagPtr[2] = tag[1];
    rawTagPtr[3] = tag[0];
    mlu = cmsMLUalloc(C->lcms, 1);
    cmsMLUsetASCII(mlu, languageCode, countryCode, ascii);
    cmsWriteTag(profile->handle, tagSignature, mlu);
    cmsMLUfree(mlu);
    clProfileReload(C, profile); // Rebuild raw and signature
    return clTrue;
}

clBool clProfileSetGamma(struct clContext * C, clProfile * profile, float gamma)
{
    cmsToneCurve * gammaCurve = cmsBuildGamma(C->lcms, gamma);

    if (!cmsWriteTag(profile->handle, cmsSigRedTRCTag, (void *)gammaCurve)) {
        goto cleanup;
    }
    if (!cmsLinkTag(profile->handle, cmsSigGreenTRCTag, cmsSigRedTRCTag)) {
        goto cleanup;
    }
    if (!cmsLinkTag(profile->handle, cmsSigBlueTRCTag, cmsSigRedTRCTag)) {
        goto cleanup;
    }
cleanup:
    cmsFreeToneCurve(gammaCurve);
    clProfileReload(C, profile); // Rebuild raw and signature
    return clTrue;
}

clBool clProfileSetLuminance(struct clContext * C, clProfile * profile, int luminance)
{
    clBool ret;
    cmsCIEXYZ lumi;
    lumi.X = 0.0f;
    lumi.Y = (cmsFloat64Number)luminance;
    lumi.Z = 0.0f;
    ret = cmsWriteTag(profile->handle, cmsSigLuminanceTag, &lumi) ? clTrue : clFalse;
    clProfileReload(C, profile); // Rebuild raw and signature
    return ret;
}

clBool clProfileRemoveTag(struct clContext * C, clProfile * profile, char * tag, const char * reason)
{
    uint8_t * tagPtr = (uint8_t *)tag;
    cmsTagSignature sig = (tagPtr[0] << 24)
                          + (tagPtr[1] << 16)
                          + (tagPtr[2] << 8)
                          + (tagPtr[3] << 0);
    if (cmsIsTag(profile->handle, sig)) {
        if (reason) {
            clContextLog(C, "modify", 0, "WARNING: Removing tag \"%s\" (%s)", tag, reason);
        }
        cmsWriteTag(profile->handle, sig, NULL);
        clProfileReload(C, profile); // Rebuild raw and signature
        return clTrue;
    }
    return clFalse;
}

clBool clProfileMatches(struct clContext * C, clProfile * profile1, clProfile * profile2)
{
    int i;
    if (profile1 == profile2) {
        return clTrue;
    } else if (!profile1 || !profile2) {
        return clFalse;
    }

    // Make sure one of them actually has a signature
    for (i = 0; i < 16; ++i) {
        if (profile1->signature[i] != 0)
            break;
        if (profile2->signature[i] != 0)
            break;
    }
    if (i == 16) {
        // No signatures, consider them not a match for now
        return clFalse;
    }
    if (!memcmp(profile1->signature, profile2->signature, 16)) {
        return clTrue;
    }
    // TODO: fallback to doing a double clProfilePack and comparison? tag comparisons?
    return clFalse;
}

clBool clProfileUsesCCMM(struct clContext * C, clProfile * profile)
{
    if (!profile)
        return C->ccmmAllowed;
    if (!C->ccmmAllowed)
        return clFalse;
    return profile->ccmm;
}

const char * clProfileCMMName(struct clContext * C, clProfile * profile)
{
    return clProfileUsesCCMM(C, profile) ? "CCMM" : "LCMS";
}

char * clGenerateDescription(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance)
{
    char * tmp = clAllocate(1024);
    sprintf(tmp, "Colorist P%g %gg %dnits", primaries->red[0], curve->gamma, maxLuminance);
    return tmp;
}
