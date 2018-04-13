// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/profile.h"

#include "colorist/context.h"

#include "lcms2_plugin.h"

#include <math.h>
#include <string.h>

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
            curve.gamma = 2.4f;
            maxLuminance = 300;
            description = "SRGB";
            break;
    }
    return clProfileCreate(C, &primaries, &curve, maxLuminance, description);
}

clProfile * clProfileClone(struct clContext * C, clProfile * profile)
{
    uint8_t * bytes;
    cmsUInt32Number bytesNeeded;
    clProfile * clone;
    if (!cmsSaveProfileToMem(profile->handle, NULL, &bytesNeeded)) {
        return NULL;
    }
    bytes = (uint8_t *)clAllocate(bytesNeeded);
    if (!cmsSaveProfileToMem(profile->handle, bytes, &bytesNeeded)) {
        clFree(bytes);
        return NULL;
    }
    clone = clAllocateStruct(clProfile);
    clone->handle = cmsOpenProfileFromMemTHR(C->lcms, bytes, bytesNeeded);
    clFree(bytes);
    if (!clone->handle) {
        clFree(clone);
        return NULL;
    }
    clone->description = profile->description ? strdup(profile->description) : NULL;
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
    return profile;
}

clProfile * clProfileCreateLinear(struct clContext * C, clProfile * origProfile)
{
    clProfile * profile;
    clProfilePrimaries primaries;
    clProfileCurve curve;
    char * description;
    int descriptionLen;
    int luminance;
    if (!clProfileQuery(C, origProfile, &primaries, NULL, &luminance)) {
        return NULL;
    }
    curve.type = CL_PCT_GAMMA;
    curve.gamma = 1.0f;
    descriptionLen = 20;
    if (origProfile->description)
        descriptionLen += strlen(origProfile->description);
    description = (char *)clAllocate(descriptionLen);
    description[0] = 0;
    if (origProfile->description)
        strcpy(description, origProfile->description);
    strcat(description, " (Linear)");
    profile = clProfileCreate(C, &primaries, &curve, luminance, description);
    clFree(description);
    return profile;
}

clBool clProfilePack(struct clContext * C, clProfile * profile, clRaw * out)
{
    cmsUInt32Number bytesNeeded;
    if (!cmsSaveProfileToMem(profile->handle, NULL, &bytesNeeded)) {
        return clFalse;
    }
    clRawRealloc(C, out, bytesNeeded);
    if (!cmsSaveProfileToMem(profile->handle, out->ptr, &bytesNeeded)) {
        clRawFree(C, out);
        return clFalse;
    }
    return clTrue;
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

void clProfileDestroy(struct clContext * C, clProfile * profile)
{
    clFree(profile->description);
    cmsCloseProfile(profile->handle);
    clFree(profile);
}

clBool clProfileQuery(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries, clProfileCurve * curve, int * luminance)
{
    if (primaries) {
        cmsMAT3 * chad;
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
        if ((redXYZ == NULL) || (greenXYZ == NULL) || (blueXYZ == NULL) || (whiteXYZ == NULL))
            return clFalse;

        _cmsVEC3init(&tmpColorants.v[0], redXYZ->X, greenXYZ->X, blueXYZ->X);
        _cmsVEC3init(&tmpColorants.v[1], redXYZ->Y, greenXYZ->Y, blueXYZ->Y);
        _cmsVEC3init(&tmpColorants.v[2], redXYZ->Z, greenXYZ->Z, blueXYZ->Z);

        chad = (cmsMAT3 *)cmsReadTag(profile->handle, cmsSigChromaticAdaptationTag);
        if ((chad != NULL) && _cmsMAT3inverse(chad, &invChad)) {
            cmsFloat64Number K;
            cmsCIExyY whiteXYY;
            cmsXYZ2xyY(&whiteXYY, whiteXYZ);
            if (cmsTempFromWhitePoint(&K, &whiteXYY) && (fabsf((float)K - 5000.0f) < 1.0f)) {
                // It's D50 in a profile with a chad tag, adapt it
                cmsVEC3 srcWP, dstWP;
                srcWP.n[VX] = whiteXYZ->X;
                srcWP.n[VY] = whiteXYZ->Y;
                srcWP.n[VZ] = whiteXYZ->Z;
                _cmsMAT3eval(&dstWP, &invChad, &srcWP);
                adaptedWhiteXYZ.X = dstWP.n[VX];
                adaptedWhiteXYZ.Y = dstWP.n[VY];
                adaptedWhiteXYZ.Z = dstWP.n[VZ];
            }
            _cmsMAT3per(&colorants, &invChad, &tmpColorants);
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
            curve->type = CL_PCT_UNKNOWN;
            curve->gamma = 0.0f;
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
    return clTrue;
}

char * clGenerateDescription(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance)
{
    char * tmp = clAllocate(1024);
    sprintf(tmp, "Colorist P%g %gg %dnits", primaries->red[0], curve->gamma, maxLuminance);
    return tmp;
}
