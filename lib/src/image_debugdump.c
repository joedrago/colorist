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

static void dumpPixel(struct clContext * C, clImage * image, clTransform * toXYZ, float maxLuminance, int x, int y, int extraIndent, cJSON * jsonPixels, clImagePixelInfo * pixelInfo);

void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent)
{
    clTransform * toXYZ = clTransformCreate(C, image->profile, CL_XF_RGBA, 32, NULL, CL_XF_XYZ, 32, CL_TONEMAP_OFF);

    clContextLog(C, "image", 0 + extraIndent, "Image: %dx%d %d-bit", image->width, image->height, image->depth);
    clProfileDebugDump(C, image->profile, C->verbose, 1 + extraIndent);

    int maxLuminance;
    clProfileQuery(C, image->profile, NULL, NULL, &maxLuminance);
    if (maxLuminance == 0) {
        maxLuminance = COLORIST_DEFAULT_LUMINANCE;
    }
    float maxLuminanceFloat = (float)maxLuminance;

    if (clImageAdjustRect(C, image, &x, &y, &w, &h)) {
        int endX = x + w;
        int endY = y + h;
        clContextLog(C, "image", 1 + extraIndent, "Pixels:");
        for (int j = y; j < endY; ++j) {
            for (int i = x; i < endX; ++i) {
                dumpPixel(C, image, toXYZ, maxLuminanceFloat, i, j, extraIndent, NULL, NULL);
            }
        }
    }

    clTransformDestroy(C, toXYZ);
}

void clImageDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clImage * image, int x, int y, int w, int h)
{
    cJSON * jsonProfile = cJSON_AddObjectToObject(jsonOutput, "profile");

    clTransform * toXYZ = clTransformCreate(C, image->profile, CL_XF_RGBA, 32, NULL, CL_XF_XYZ, 32, CL_TONEMAP_OFF);

    cJSON_AddNumberToObject(jsonOutput, "width", image->width);
    cJSON_AddNumberToObject(jsonOutput, "height", image->height);
    cJSON_AddNumberToObject(jsonOutput, "depth", image->depth);

    clProfileDebugDumpJSON(C, jsonProfile, image->profile, C->verbose);

    int maxLuminance;
    clProfileQuery(C, image->profile, NULL, NULL, &maxLuminance);
    if (maxLuminance == 0) {
        maxLuminance = COLORIST_DEFAULT_LUMINANCE;
    }
    float maxLuminanceFloat = (float)maxLuminance;

    if (clImageAdjustRect(C, image, &x, &y, &w, &h)) {
        int endX = x + w;
        int endY = y + h;
        cJSON * jsonPixels = NULL;
        for (int j = y; j < endY; ++j) {
            for (int i = x; i < endX; ++i) {
                if (!jsonPixels) {
                    // Lazily create it in case we never have to
                    jsonPixels = cJSON_AddArrayToObject(jsonOutput, "pixels");
                }
                dumpPixel(C, image, toXYZ, maxLuminanceFloat, i, j, 0, jsonPixels, NULL);
            }
        }
    }

    clTransformDestroy(C, toXYZ);
}

void clImageDebugDumpPixel(struct clContext * C, clImage * image, int x, int y, clImagePixelInfo * pixelInfo)
{
    if ((x < 0) || (x >= image->width) || (y < 0) || (y >= image->height)) {
        memset(pixelInfo, 0, sizeof(pixelInfo));
        return;
    }

    clTransform * toXYZ = clTransformCreate(C, image->profile, CL_XF_RGBA, 32, NULL, CL_XF_XYZ, 32, CL_TONEMAP_OFF);

    int maxLuminance;
    clProfileQuery(C, image->profile, NULL, NULL, &maxLuminance);
    if (maxLuminance == 0) {
        maxLuminance = COLORIST_DEFAULT_LUMINANCE;
    }
    float maxLuminanceFloat = (float)maxLuminance;

    dumpPixel(C, image, toXYZ, maxLuminanceFloat, x, y, 0, NULL, pixelInfo);

    clTransformDestroy(C, toXYZ);
}

static void dumpPixel(struct clContext * C, clImage * image, clTransform * toXYZ, float maxLuminance, int x, int y, int extraIndent, cJSON * jsonPixels, clImagePixelInfo * pixelInfo)
{
    uint16_t intRGB[4];
    float maxChannel = (float)((1 << image->depth) - 1);
    float floatRGBA[4];
    float floatXYZ[3];
    cmsCIEXYZ XYZ;
    cmsCIExyY xyY;

    COLORIST_ASSERT(image->pixels);

    uint16_t * pixel = &image->pixels[CL_CHANNELS_PER_PIXEL * (x + (y * image->width))];
    intRGB[0] = pixel[0];
    intRGB[1] = pixel[1];
    intRGB[2] = pixel[2];
    intRGB[3] = pixel[3];

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
        cJSON_AddNumberToObject(t, "X", XYZ.X / maxLuminance);
        cJSON_AddNumberToObject(t, "Y", XYZ.Y / maxLuminance);
        cJSON_AddNumberToObject(t, "Z", XYZ.Z / maxLuminance);

        t = cJSON_AddObjectToObject(jsonPixel, "xyY");
        cJSON_AddNumberToObject(t, "x", xyY.x);
        cJSON_AddNumberToObject(t, "y", xyY.y);
        cJSON_AddNumberToObject(t, "Y", xyY.Y / maxLuminance);

        cJSON_AddNumberToObject(jsonPixel, "nits", xyY.Y * maxLuminance);

        cJSON_AddItemToArray(jsonPixels, jsonPixel);
    } else if (pixelInfo) {
        pixelInfo->rawR = intRGB[0];
        pixelInfo->rawG = intRGB[1];
        pixelInfo->rawB = intRGB[2];
        pixelInfo->rawA = intRGB[3];
        pixelInfo->normR = floatRGBA[0];
        pixelInfo->normG = floatRGBA[1];
        pixelInfo->normB = floatRGBA[2];
        pixelInfo->normA = floatRGBA[3];
        pixelInfo->X = (float)XYZ.X;
        pixelInfo->Y = (float)XYZ.Y;
        pixelInfo->Z = (float)XYZ.Z;
        pixelInfo->x = (float)xyY.x;
        pixelInfo->y = (float)xyY.y;
    } else {
        clContextLog(C, "image", 2 + extraIndent, "Pixel(%d, %d): rgba%d(%u, %u, %u, %u), f(%g, %g, %g, %g), XYZ(%g, %g, %g), xyY(%g, %g, %g), %g nits",
            x, y, image->depth,
            intRGB[0], intRGB[1], intRGB[2], intRGB[3],
            floatRGBA[0], floatRGBA[1], floatRGBA[2], floatRGBA[3],
            XYZ.X / maxLuminance, XYZ.Y / maxLuminance, XYZ.Z / maxLuminance,
            xyY.x, xyY.y, xyY.Y / maxLuminance,
            xyY.Y);
    }
}
