// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/transform.h"

#include <string.h>

clImageDiff * clImageDiffCreate(struct clContext * C, clImage * image1, clImage * image2, int taskCount, float minIntensity, int threshold)
{
    if (!clProfileMatches(C, image1->profile, image2->profile) ||
        (image1->width != image2->width) ||
        (image1->height != image2->height) ||
        (image1->depth != image2->depth))
    {
        return NULL;
    }

    clImageDiff * diff = clAllocateStruct(clImageDiff);
    memset(diff, 0, sizeof(clImageDiff));
    diff->pixelCount = image1->width * image1->height;
    diff->minIntensity = minIntensity;
    diff->image = clImageCreate(C, image1->width, image1->height, 8, NULL);
    diff->diffs = clAllocate(sizeof(uint16_t) * diff->pixelCount);
    diff->intensities = clAllocate(sizeof(uint16_t) * diff->pixelCount);

    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance = CL_LUMINANCE_UNSPECIFIED;
    clProfileQuery(C, image1->profile, &primaries, &curve, &luminance);
    if (luminance == CL_LUMINANCE_UNSPECIFIED) {
        if (curve.type == CL_PCT_HLG) {
            luminance = clTransformCalcHLGLuminance(C->defaultLuminance);
        } else {
            luminance = C->defaultLuminance;
        }
    }

    clContextGetStockPrimaries(C, "bt709", &primaries);
    curve.type = CL_PCT_GAMMA;
    curve.implicitScale = 1.0f;
    curve.gamma = 1.0f;
    clProfile * intensityProfile = clProfileCreate(C, &primaries, &curve, C->defaultLuminance, NULL);
    clImage * intensityMap = clImageConvert(C, image1, taskCount, 16, intensityProfile, CL_TONEMAP_AUTO);
    clProfileDestroy(C, intensityProfile);

    float kr = 0.2126f;
    float kb = 0.0722f;
    float kg = 1.0f - kr - kb;
    for (int i = 0; i < diff->pixelCount; ++i) {
        uint16_t * p1 = &image1->pixels[i * CL_CHANNELS_PER_PIXEL];
        uint16_t * p2 = &image2->pixels[i * CL_CHANNELS_PER_PIXEL];
        uint16_t * diffPixel = &diff->image->pixels[i * CL_CHANNELS_PER_PIXEL];

        uint16_t * intensityPixel = &intensityMap->pixels[i * CL_CHANNELS_PER_PIXEL];
        float intensity = ((intensityPixel[0] / 65535.0f) * kr) +
                          ((intensityPixel[1] / 65535.0f) * kg) +
                          ((intensityPixel[2] / 65535.0f) * kb);
        intensity = CL_CLAMP(intensity + diff->minIntensity, 0.0f, 1.0f);
        diff->intensities[i] = (uint16_t)clPixelMathRoundf(255.0f * powf(intensity, 1.0f / 2.2f));

        int channelDiff;
        int largestDiff = abs((int)p1[0] - (int)p2[0]);
        channelDiff = abs((int)p1[1] - (int)p2[1]);
        if (largestDiff < channelDiff) {
            largestDiff = channelDiff;
        }
        channelDiff = abs((int)p1[2] - (int)p2[2]);
        if (largestDiff < channelDiff) {
            largestDiff = channelDiff;
        }
        channelDiff = abs((int)p1[3] - (int)p2[3]);
        if (largestDiff < channelDiff) {
            largestDiff = channelDiff;
        }

        diff->diffs[i] = (uint16_t)largestDiff;

        if (diff->largestChannelDiff < largestDiff) {
            diff->largestChannelDiff = largestDiff;
        }

        diffPixel[3] = 255;
    }

    clImageDestroy(C, intensityMap);

    clImageDiffUpdate(C, diff, threshold);
    return diff;
}

void clImageDiffUpdate(struct clContext * C, clImageDiff * diff, int threshold)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(threshold);

    diff->matchCount = 0;
    diff->underThresholdCount = 0;
    diff->overThresholdCount = 0;

    for (int i = 0; i < diff->pixelCount; ++i) {
        uint16_t * diffPixel = &diff->image->pixels[i * CL_CHANNELS_PER_PIXEL];

        if (diff->diffs[i] == 0) {
            ++diff->matchCount;
            diffPixel[0] = diff->intensities[i];
            diffPixel[1] = diff->intensities[i];
            diffPixel[2] = diff->intensities[i];
        } else if (diff->diffs[i] <= threshold) {
            ++diff->underThresholdCount;
            diffPixel[0] = diff->intensities[i] >> 4;
            diffPixel[1] = diff->intensities[i] >> 4;
            diffPixel[2] = diff->intensities[i];
        } else {
            ++diff->overThresholdCount;
            diffPixel[0] = diff->intensities[i];
            diffPixel[1] = diff->intensities[i] >> 4;
            diffPixel[2] = diff->intensities[i] >> 4;
        }
    }
}

void clImageDiffDestroy(struct clContext * C, clImageDiff * diff)
{
    clImageDestroy(C, diff->image);
    clFree(diff->diffs);
    clFree(diff->intensities);
    clFree(diff);
}
