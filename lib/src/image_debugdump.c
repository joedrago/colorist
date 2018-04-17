// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

static void dumpPixel(struct clContext * C, clImage * image, cmsHTRANSFORM toXYZ, int x, int y, int extraIndent);

void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent)
{
    int i, j;
    int dumpEndW;
    int dumpEndH;

    cmsHPROFILE xyzProfile = cmsCreateXYZProfileTHR(C->lcms);
    cmsHTRANSFORM toXYZ = cmsCreateTransformTHR(C->lcms, image->profile->handle, TYPE_RGB_FLT, xyzProfile, TYPE_XYZ_FLT, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE);

    dumpEndW = x + w;
    dumpEndH = y + h;
    dumpEndW = (dumpEndW < image->width) ? dumpEndW : image->width;
    dumpEndH = (dumpEndH < image->height) ? dumpEndH : image->height;

    clContextLog(C, "image", 0 + extraIndent, "Image: %dx%d %d-bit", image->width, image->height, image->depth);
    clProfileDebugDump(C, image->profile, 1 + extraIndent);
    if (((dumpEndW - x) > 0) && ((dumpEndH - y) > 0)) {
        clContextLog(C, "image", 1 + extraIndent, "Pixels:");
    }

    for (j = y; j < dumpEndH; ++j) {
        for (i = x; i < dumpEndW; ++i) {
            dumpPixel(C, image, toXYZ, i, j, extraIndent);
        }
    }

    cmsDeleteTransform(toXYZ);
    cmsCloseProfile(xyzProfile);
}

static void dumpPixel(struct clContext * C, clImage * image, cmsHTRANSFORM toXYZ, int x, int y, int extraIndent)
{
    COLORIST_ASSERT(image->pixels);
    int intRGB[4];
    float maxChannel;
    float floatRGB[4];
    float floatXYZ[3];
    cmsCIEXYZ XYZ;
    cmsCIExyY xyY;

    if (image->depth == 16) {
        uint16_t * shorts = (uint16_t *)image->pixels;
        uint16_t * pixel = &shorts[4 * (x + (y * image->width))];
        intRGB[0] = pixel[0];
        intRGB[1] = pixel[1];
        intRGB[2] = pixel[2];
        intRGB[3] = pixel[3];
        maxChannel = 65535.0f;
    } else {
        uint8_t * pixel = &image->pixels[4 * (x + (y * image->width))];
        COLORIST_ASSERT(image->depth == 8);
        intRGB[0] = pixel[0];
        intRGB[1] = pixel[1];
        intRGB[2] = pixel[2];
        intRGB[3] = pixel[3];
        maxChannel = 255.0f;
    }

    floatRGB[0] = intRGB[0] / maxChannel;
    floatRGB[1] = intRGB[1] / maxChannel;
    floatRGB[2] = intRGB[2] / maxChannel;
    floatRGB[3] = intRGB[3] / maxChannel;
    cmsDoTransform(toXYZ, floatRGB, floatXYZ, 1);
    XYZ.X = floatXYZ[0];
    XYZ.Y = floatXYZ[1];
    XYZ.Z = floatXYZ[2];
    cmsXYZ2xyY(&xyY, &XYZ);

    clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): rgba%d(%u, %u, %u, %u), XYZ(%g, %g, %g), xyY(%g, %g, %g)",
        x, y, image->depth,
        intRGB[0], intRGB[1], intRGB[2], intRGB[3],
        XYZ.X, XYZ.Y, XYZ.Z,
        xyY.x, xyY.y, xyY.Y);
}
