#include "colorist/colorist.h"

#include "lcms2.h"

#include <stdio.h>

cmsHPROFILE createDstLinearProfile(cmsHPROFILE srcProfile)
{
    cmsHPROFILE outProfile;
    cmsCIEXYZ * dstRXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigRedColorantTag);
    cmsCIEXYZ * dstGXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigGreenColorantTag);
    cmsCIEXYZ * dstBXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigBlueColorantTag);
    cmsCIEXYZ * dstWXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigMediaWhitePointTag);
    cmsToneCurve * gamma1 = cmsBuildGamma(NULL, 1.0);
    cmsToneCurve * curves[3];
    cmsCIExyYTRIPLE dstPrimaries;
    cmsCIExyY dstWhitePoint;
    cmsXYZ2xyY(&dstPrimaries.Red, dstRXYZ);
    cmsXYZ2xyY(&dstPrimaries.Green, dstGXYZ);
    cmsXYZ2xyY(&dstPrimaries.Blue, dstBXYZ);
    cmsXYZ2xyY(&dstWhitePoint, dstWXYZ);
    curves[0] = gamma1;
    curves[1] = gamma1;
    curves[2] = gamma1;
    outProfile = cmsCreateRGBProfile(&dstWhitePoint, &dstPrimaries, curves);
    cmsFreeToneCurve(gamma1);
    return outProfile;
}

int main(int argc, char * argv[])
{
    // {
    //     clImage * image = clImageCreate(2, 2, 8, NULL);
    //     clImageSetPixel(image, 0, 0, 255, 128, 238, 0);
    //     clImageDebugDump(image);
    //     clImageChangeDepth(image, 16);
    //     clImageDebugDump(image);
    //     clImageChangeDepth(image, 8);
    //     clImageDebugDump(image);
    //     clImageDestroy(image);
    // }
    // {
    //     clImage * image;
    //     clProfile * profile;
    //     profile = clProfileParse((const uint8_t *)"foo", 3);
    //     clProfileDestroy(profile);
    // }
    {
        cmsHPROFILE srcProfile = cmsOpenProfileFromFile("f:\\work\\hdr10.icc", "r");
        cmsHPROFILE dstProfile = cmsOpenProfileFromFile("f:\\work\\srgb.icc", "r");
        cmsHPROFILE dstProfileLin = createDstLinearProfile(dstProfile);
        cmsHTRANSFORM toLinear = cmsCreateTransform(srcProfile, TYPE_RGBA_8, dstProfileLin, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);
        cmsHTRANSFORM fromLinear = cmsCreateTransform(dstProfileLin, TYPE_RGBA_FLT, dstProfile, TYPE_RGBA_8, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);
        uint8_t srcPixel[4] = { 224, 145, 72, 255 };
        float dstPixelLin[4];
        uint8_t dstPixel[4];
        cmsDoTransform(toLinear, srcPixel, dstPixelLin, 1);
        cmsDoTransform(fromLinear, dstPixelLin, dstPixel, 1);
        cmsCloseProfile(srcProfile);
        cmsCloseProfile(dstProfile);
        cmsCloseProfile(toLinear);
    }
    return 0;
}
