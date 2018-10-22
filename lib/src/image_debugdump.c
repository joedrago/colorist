// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include "cJSON.h"

#include <string.h>

static void dumpPixel(struct clContext * C, clImage * image, clTransform * toXYZ, float maxLuminance, int x, int y, int extraIndent, cJSON * jsonPixels);

void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent)
{
    int i, j;
    int maxLuminance;
    float maxLuminanceFloat;

    clTransform * toXYZ = clTransformCreate(C, image->profile, CL_TF_RGBA_FLOAT, NULL, CL_TF_XYZ_FLOAT);

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
                dumpPixel(C, image, toXYZ, maxLuminanceFloat, i, j, extraIndent, NULL);
            }
        }
    }

    clTransformDestroy(C, toXYZ);
}

void clImageDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clImage * image, int x, int y, int w, int h)
{
    cJSON * jsonProfile = cJSON_AddObjectToObject(jsonOutput, "profile");

    int i, j;
    int maxLuminance;
    float maxLuminanceFloat;

    clTransform * toXYZ = clTransformCreate(C, image->profile, CL_TF_RGBA_FLOAT, NULL, CL_TF_XYZ_FLOAT);

    cJSON_AddNumberToObject(jsonOutput, "width", image->width);
    cJSON_AddNumberToObject(jsonOutput, "height", image->height);
    cJSON_AddNumberToObject(jsonOutput, "depth", image->depth);

    clProfileDebugDumpJSON(C, jsonProfile, image->profile, C->verbose);

    clProfileQuery(C, image->profile, NULL, NULL, &maxLuminance);
    if (maxLuminance == 0) {
        maxLuminance = COLORIST_DEFAULT_LUMINANCE;
    }
    maxLuminanceFloat = (float)maxLuminance;

    if (clImageAdjustRect(C, image, &x, &y, &w, &h)) {
        int endX = x + w;
        int endY = y + h;
        cJSON * jsonPixels = NULL;
        for (j = y; j < endY; ++j) {
            for (i = x; i < endX; ++i) {
                if (!jsonPixels) {
                    // Lazily create it in case we never have to
                    jsonPixels = cJSON_AddArrayToObject(jsonOutput, "pixels");
                }
                dumpPixel(C, image, toXYZ, maxLuminanceFloat, i, j, 0, jsonPixels);
            }
        }
    }

    clTransformDestroy(C, toXYZ);
}

static void dumpPixel(struct clContext * C, clImage * image, clTransform * toXYZ, float maxLuminance, int x, int y, int extraIndent, cJSON * jsonPixels)
{
    int intRGB[4];
    float maxChannel = (float)((1 << image->depth) - 1);
    float floatRGBA[4];
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

    floatRGBA[0] = intRGB[0] / maxChannel;
    floatRGBA[1] = intRGB[1] / maxChannel;
    floatRGBA[2] = intRGB[2] / maxChannel;
    floatRGBA[3] = intRGB[3] / maxChannel;
    clTransformRun(C, toXYZ, 1, floatRGBA, floatXYZ, 1);
    XYZ.X = floatXYZ[0];
    XYZ.Y = floatXYZ[1];
    XYZ.Z = floatXYZ[2];
    if (XYZ.Y > 0) {
        cmsXYZ2xyY(&xyY, &XYZ);
    } else {
        // This is wrong, xy should be the white point
        memset(&xyY, 0, sizeof(xyY));
    }

    if (jsonPixels) {
        cJSON * jsonPixel = cJSON_CreateObject();
        cJSON * t;

        cJSON_AddNumberToObject(jsonPixel, "x", x);
        cJSON_AddNumberToObject(jsonPixel, "y", y);

        t = cJSON_AddObjectToObject(jsonPixel, "raw");
        cJSON_AddNumberToObject(t, "r", intRGB[0]);
        cJSON_AddNumberToObject(t, "g", intRGB[1]);
        cJSON_AddNumberToObject(t, "b", intRGB[2]);
        cJSON_AddNumberToObject(t, "a", intRGB[3]);

        t = cJSON_AddObjectToObject(jsonPixel, "float");
        cJSON_AddNumberToObject(t, "r", floatRGBA[0]);
        cJSON_AddNumberToObject(t, "g", floatRGBA[1]);
        cJSON_AddNumberToObject(t, "b", floatRGBA[2]);
        cJSON_AddNumberToObject(t, "a", floatRGBA[3]);

        t = cJSON_AddObjectToObject(jsonPixel, "XYZ");
        cJSON_AddNumberToObject(t, "X", XYZ.X);
        cJSON_AddNumberToObject(t, "Y", XYZ.Y);
        cJSON_AddNumberToObject(t, "Z", XYZ.Z);

        t = cJSON_AddObjectToObject(jsonPixel, "xyY");
        cJSON_AddNumberToObject(t, "x", xyY.x);
        cJSON_AddNumberToObject(t, "y", xyY.y);
        cJSON_AddNumberToObject(t, "Y", xyY.Y);

        cJSON_AddNumberToObject(jsonPixel, "nits", xyY.Y * maxLuminance);

        cJSON_AddItemToArray(jsonPixels, jsonPixel);
    } else {
        clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): rgba%d(%u, %u, %u, %u), f(%g, %g, %g, %g), XYZ(%g, %g, %g), xyY(%g, %g, %g), %g nits",
            x, y, image->depth,
            intRGB[0], intRGB[1], intRGB[2], intRGB[3],
            floatRGBA[0], floatRGBA[1], floatRGBA[2], floatRGBA[3],
            XYZ.X, XYZ.Y, XYZ.Z,
            xyY.x, xyY.y, xyY.Y,
            xyY.Y * maxLuminance);
    }
}
