// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include "lcms2_plugin.h"

#define COLORIST_DUMP_DIM 4

// From lcms2_internal.h
#define MAX_ENCODEABLE_XYZ  (1.0 + 32767.0 / 32768.0)
#define InpAdj   (1.0 / MAX_ENCODEABLE_XYZ) // (65536.0/(65535.0*2.0))

static void dumpPixel(struct clContext * C, clImage * image, float gamma, cmsMAT3 * rgbToXYZ, int x, int y, int extraIndent);

void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent)
{
    int i, j;
    int dumpEndW;
    int dumpEndH;

    cmsCIEXYZ * rTag;
    cmsCIEXYZ * gTag;
    cmsCIEXYZ * bTag;
    cmsMAT3 rgbToXYZ;
    clProfileCurve curve;
    float gamma;

    dumpEndW = x + w;
    dumpEndH = y + h;
    dumpEndW = (dumpEndW < image->width) ? dumpEndW : image->width;
    dumpEndH = (dumpEndH < image->height) ? dumpEndH : image->height;

    clContextLog(C, "image", 0 + extraIndent, "Image: %dx%d %d-bit", image->width, image->height, image->depth);
    clProfileDebugDump(C, image->profile, 1 + extraIndent);
    if (((dumpEndW - x) > 0) && ((dumpEndH - y) > 0)) {
        clContextLog(C, "image", 1 + extraIndent, "Pixels:");
    }

    // Borrowed/adapted from cmsio1.c
    // This will be with respect to D50, but I just want the xy coordinate so I don't care
    rTag = (cmsCIEXYZ *)cmsReadTag(image->profile->handle, cmsSigRedColorantTag);
    gTag = (cmsCIEXYZ *)cmsReadTag(image->profile->handle, cmsSigGreenColorantTag);
    bTag = (cmsCIEXYZ *)cmsReadTag(image->profile->handle, cmsSigBlueColorantTag);
    if ((rTag == NULL) || (gTag == NULL) || (bTag == NULL)) {
        clContextLogError(C, "Missing colorant tags in image profile!");
        return;
    }
    _cmsVEC3init(&rgbToXYZ.v[0], rTag->X, gTag->X, bTag->X);
    _cmsVEC3init(&rgbToXYZ.v[1], rTag->Y, gTag->Y, bTag->Y);
    _cmsVEC3init(&rgbToXYZ.v[2], rTag->Z, gTag->Z, bTag->Z);
    for (i=0; i < 3; i++)
        for (j=0; j < 3; j++)
            rgbToXYZ.v[i].n[j] *= InpAdj;

    gamma = 0.0f;
    if (clProfileQuery(C, image->profile, NULL, &curve, NULL)) {
        if (curve.type == CL_PCT_GAMMA) {
            gamma = curve.gamma;
        }
    }

    for (j = y; j < dumpEndH; ++j) {
        for (i = x; i < dumpEndW; ++i) {
            dumpPixel(C, image, gamma, &rgbToXYZ, i, j, extraIndent);
        }
        // clContextLog(C, "image", 0 + extraIndent, "");
    }
}

// TODO: rework this ugly function by distributing out copypasta, amongst other changes
static void dumpPixel(struct clContext * C, clImage * image, float gamma, cmsMAT3 * rgbToXYZ, int x, int y, int extraIndent)
{
    COLORIST_ASSERT(image->pixels);
    cmsVEC3 rgbFloat;
    cmsVEC3 xyzFloat;
    cmsCIEXYZ XYZ;
    cmsCIExyY xyY;
    if (image->depth == 16) {
        uint16_t * shorts = (uint16_t *)image->pixels;
        uint16_t * pixel = &shorts[4 * (x + (y * image->width))];
        if (gamma > 0.0f) {
            rgbFloat.n[VX] = powf((float)pixel[0] / 65535.0f, gamma);
            rgbFloat.n[VY] = powf((float)pixel[1] / 65535.0f, gamma);
            rgbFloat.n[VZ] = powf((float)pixel[2] / 65535.0f, gamma);
            _cmsMAT3eval(&xyzFloat, rgbToXYZ, &rgbFloat);
            XYZ.X = xyzFloat.n[VX];
            XYZ.Y = xyzFloat.n[VY];
            XYZ.Z = xyzFloat.n[VZ];
            cmsXYZ2xyY(&xyY, &XYZ);
        } else {
            // Unsupported
            memset(&xyY, 0, sizeof(xyY));
        }
        clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): (%u, %u, %u, %u) -> (%g, %g, %g, %g), xy(%g, %g)",
            x, y,
            (unsigned int)pixel[0], (unsigned int)pixel[1], (unsigned int)pixel[2], (unsigned int)pixel[3],
            (float)pixel[0] / 65535.0f, (float)pixel[1] / 65535.0f, (float)pixel[2] / 65535.0f, (float)pixel[3] / 65535.0f,
            xyY.x, xyY.y);
    } else {
        uint8_t * pixel = &image->pixels[4 * (x + (y * image->width))];
        COLORIST_ASSERT(image->depth == 8);
        if (gamma > 0.0f) {
            rgbFloat.n[VX] = powf((float)pixel[0] / 255.0f, gamma);
            rgbFloat.n[VY] = powf((float)pixel[1] / 255.0f, gamma);
            rgbFloat.n[VZ] = powf((float)pixel[2] / 255.0f, gamma);
            _cmsMAT3eval(&xyzFloat, rgbToXYZ, &rgbFloat);
            XYZ.X = xyzFloat.n[VX];
            XYZ.Y = xyzFloat.n[VY];
            XYZ.Z = xyzFloat.n[VZ];
            cmsXYZ2xyY(&xyY, &XYZ);
        } else {
            // Unsupported
            memset(&xyY, 0, sizeof(xyY));
        }
        clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): (%u, %u, %u, %u) -> (%g, %g, %g, %g), xy(%g, %g)",
            x, y,
            (unsigned int)pixel[0], (unsigned int)pixel[1], (unsigned int)pixel[2], (unsigned int)pixel[3],
            (float)pixel[0] / 255.0f, (float)pixel[1] / 255.0f, (float)pixel[2] / 255.0f, (float)pixel[3] / 255.0f,
            xyY.x, xyY.y);
    }
}
