// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include <string.h>

static void dumpPixel(struct clContext * C, clImage * image, cmsHTRANSFORM toXYZ, float maxLuminance, int x, int y, int extraIndent);

void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent)
{
    int i, j;
    int maxLuminance;
    float maxLuminanceFloat;

    cmsHPROFILE xyzProfile = cmsCreateXYZProfileTHR(C->lcms);
    cmsHTRANSFORM toXYZ = cmsCreateTransformTHR(C->lcms, image->profile->handle, TYPE_RGB_FLT, xyzProfile, TYPE_XYZ_FLT, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE);

    clContextLog(C, "image", 0 + extraIndent, "Image: %dx%d %d-bit", image->width, image->height, image->depth);
    clProfileDebugDump(C, image->profile, C->verbose, 1 + extraIndent);

    clProfileQuery(C, image->profile, NULL, NULL, &maxLuminance);
    if (maxLuminance == 0) {
        maxLuminance = COLORIST_DEFAULT_LUMINANCE;
    }
    maxLuminanceFloat = (float)maxLuminance;

    if (clImageAdjustRect(C, image, &x, &y, &w, &h)) {
        int endX = x + w;
        int endY = y + h;
        clContextLog(C, "image", 1 + extraIndent, "Pixels:");
        for (j = y; j < endY; ++j) {
            for (i = x; i < endX; ++i) {
                dumpPixel(C, image, toXYZ, maxLuminanceFloat, i, j, extraIndent);
            }
        }
    }

    cmsDeleteTransform(toXYZ);
    cmsCloseProfile(xyzProfile);
}

static void dumpPixel(struct clContext * C, clImage * image, cmsHTRANSFORM toXYZ, float maxLuminance, int x, int y, int extraIndent)
{
    int intRGB[4];
    float maxChannel = (float)((1 << image->depth) - 1);
    float floatRGB[4];
    float floatXYZ[3];
    cmsCIEXYZ XYZ;
    cmsCIExyY xyY;

    COLORIST_ASSERT(image->pixels);

    if (image->depth > 8) {
        uint16_t * shorts = (uint16_t *)image->pixels;
        uint16_t * pixel = &shorts[4 * (x + (y * image->width))];
        intRGB[0] = pixel[0];
        intRGB[1] = pixel[1];
        intRGB[2] = pixel[2];
        intRGB[3] = pixel[3];
    } else {
        uint8_t * pixel = &image->pixels[4 * (x + (y * image->width))];
        intRGB[0] = pixel[0];
        intRGB[1] = pixel[1];
        intRGB[2] = pixel[2];
        intRGB[3] = pixel[3];
    }

    floatRGB[0] = intRGB[0] / maxChannel;
    floatRGB[1] = intRGB[1] / maxChannel;
    floatRGB[2] = intRGB[2] / maxChannel;
    floatRGB[3] = intRGB[3] / maxChannel;
    cmsDoTransform(toXYZ, floatRGB, floatXYZ, 1);
    XYZ.X = floatXYZ[0];
    XYZ.Y = floatXYZ[1];
    XYZ.Z = floatXYZ[2];
    if (XYZ.Y > 0) {
        cmsXYZ2xyY(&xyY, &XYZ);
    } else {
        // This is wrong, xy should be the white point
        memset(&xyY, 0, sizeof(xyY));
    }

    clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): rgba%d(%u, %u, %u, %u), f(%g, %g, %g, %g), XYZ(%g, %g, %g), xyY(%g, %g, %g), %g nits",
        x, y, image->depth,
        intRGB[0], intRGB[1], intRGB[2], intRGB[3],
        floatRGB[0], floatRGB[1], floatRGB[2], floatRGB[3],
        XYZ.X, XYZ.Y, XYZ.Z,
        xyY.x, xyY.y, xyY.Y,
        xyY.Y * maxLuminance);
}
