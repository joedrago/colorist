// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/image.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/task.h"

#include <string.h>

#define FAIL() { result = clFalse; goto convertCleanup; }

void doMultithreadedTransform(clContext * C, int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount);

struct ImageInfo
{
    int width;
    int height;
    int depth;

    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance;
};

clImage * clImageConvert(struct clContext * C, clImage * srcImage, struct clConversionParams * params)
{
    // -----------------------------------------------------------------------
    // Variables

    // Global state / timing
    clBool result = clTrue;
    Timer t;

    // Goals
    clProfile * dstProfile = NULL;
    clImage * dstImage = NULL;

    // Information about the src&dst images, used to make all decisions
    struct ImageInfo srcInfo;
    struct ImageInfo dstInfo;
    clBool needsColorConversion, needsResize, needsFloatRedepth, needsSrcFloats, needsLinearPixels;

    // Hald CLUT
    clImage * haldImage = NULL;
    int haldDims = 0;

    // Intermediate pixel data
    int pixelCount = srcImage->width * srcImage->height;
    float * srcFloatsPixels = NULL;         // source values in normalized floating point, manually created to avoid cms eval'ing on a 16-bit basis for floats (yuck)
    float * linearFloatsPixels = NULL;      // destination values in linear space
    clProfile * linearFloatsProfile = NULL; // profile for linearFloatsPixels

    // -----------------------------------------------------------------------
    // Parse source image and conversion params, make decisions about dst

    // Populate srcInfo
    srcInfo.width = srcImage->width;
    srcInfo.height = srcImage->height;
    srcInfo.depth = srcImage->depth;
    clProfileQuery(C, srcImage->profile, &srcInfo.primaries, &srcInfo.curve, &srcInfo.luminance);
    srcInfo.luminance = (srcInfo.luminance != 0) ? srcInfo.luminance : COLORIST_DEFAULT_LUMINANCE;
    if ((srcInfo.curve.type != CL_PCT_GAMMA) && (srcInfo.curve.gamma > 0.0f)) {
        clContextLog(C, "info", 0, "Estimated source gamma: %g", srcInfo.curve.gamma);
    }

    // Start off dstInfo with srcInfo's values
    memcpy(&dstInfo, &srcInfo, sizeof(dstInfo));

    // Forget starting gamma and luminance if we're autograding (conversion params can still force values)
    if (params->autoGrade) {
        dstInfo.curve.type = CL_PCT_GAMMA;
        dstInfo.curve.gamma = 0;
        dstInfo.luminance = 0;
    }

    // Load output profile override, if any
    if (params->iccOverrideOut) {
        if (params->autoGrade) {
            clContextLogError(C, "Can't autograde (-a) along with a specified profile from disk (--iccout), please choose one or the other.");
            FAIL();
        }

        dstProfile = clProfileRead(C, params->iccOverrideOut);
        if (!dstProfile) {
            clContextLogError(C, "Invalid destination profile override: %s", params->iccOverrideOut);
            FAIL();
        }

        clProfileQuery(C, dstProfile, &dstInfo.primaries, &dstInfo.curve, &dstInfo.luminance);
        dstInfo.luminance = (dstInfo.luminance != 0) ? dstInfo.luminance : COLORIST_DEFAULT_LUMINANCE;
        if ((dstInfo.curve.type != CL_PCT_GAMMA) && (dstInfo.curve.gamma > 0.0f)) {
            clContextLog(C, "info", 0, "Estimated dst gamma: %g", dstInfo.curve.gamma);
        }

        clContextLog(C, "profile", 1, "Overriding dst profile with file: %s", params->iccOverrideOut);
    } else {
        // No output profile, allow profile overrides

        // Override primaries
        if (params->primaries[0] > 0.0f) {
            dstInfo.primaries.red[0] = params->primaries[0];
            dstInfo.primaries.red[1] = params->primaries[1];
            dstInfo.primaries.green[0] = params->primaries[2];
            dstInfo.primaries.green[1] = params->primaries[3];
            dstInfo.primaries.blue[0] = params->primaries[4];
            dstInfo.primaries.blue[1] = params->primaries[5];
            dstInfo.primaries.white[0] = params->primaries[6];
            dstInfo.primaries.white[1] = params->primaries[7];
        }

        // Override luminance
        if (params->luminance > 0) {
            dstInfo.luminance = params->luminance;
        }

        // Override gamma
        if (params->gamma > 0.0f) {
            dstInfo.curve.type = CL_PCT_GAMMA;
            dstInfo.curve.gamma = params->gamma;
        }
    }

    // Override width and height
    if ((params->resizeW > 0) || (params->resizeH > 0)) {
        if (params->resizeW <= 0) {
            dstInfo.width = (int)(((float)srcInfo.width / (float)srcInfo.height) * params->resizeH);
            dstInfo.height = params->resizeH;
        } else if (params->resizeH <= 0) {
            dstInfo.width = params->resizeW;
            dstInfo.height = (int)(((float)srcInfo.height / (float)srcInfo.width) * params->resizeW);
        } else {
            dstInfo.width = params->resizeW;
            dstInfo.height = params->resizeH;
        }
        if (dstInfo.width <= 0)
            dstInfo.width = 1;
        if (dstInfo.height <= 0)
            dstInfo.height = 1;
    }

    // Override depth
    {
        int bestDepth;
        if (params->bpp > 0) {
            dstInfo.depth = params->bpp;
        }
        bestDepth = clFormatBestDepth(C, params->formatName, dstInfo.depth);
        if (dstInfo.depth != bestDepth) {
            clContextLog(C, "validate", 0, "Overriding output depth %d-bit -> %d-bit (format limitations)", dstInfo.depth, bestDepth);
            dstInfo.depth = bestDepth;
        }
    }

    // Load HALD, if any
    if (params->hald) {
        haldImage = clContextRead(C, params->hald, NULL, NULL);
        if (!haldImage) {
            clContextLogError(C, "Can't read Hald CLUT: %s", params->hald);
            FAIL();
        }
        if (haldImage->width != haldImage->height) {
            clContextLogError(C, "Hald CLUT isn't square [%dx%d]: %s", haldImage->width, haldImage->height, params->hald);
            FAIL();
        }

        // Calc haldDims
        {
            int i;
            for (i = 0; i < 32; ++i) {
                if ((i * i * i) == haldImage->width) {
                    haldDims = i * i;
                    break;
                }
            }

            if (haldDims == 0) {
                clContextLogError(C, "Hald CLUT dimensions aren't cubic [%dx%d]: %s", haldImage->width, haldImage->height, params->hald);
                FAIL();
            }

            clContextLog(C, "hald", 0, "Loaded %dx%dx%d Hald CLUT: %s", haldDims, haldDims, haldDims, params->hald);
        }
    }

    // -----------------------------------------------------------------------
    // Create intermediate pixels sets, if necessary

    {
        needsColorConversion = (
            memcmp(&srcInfo.primaries, &dstInfo.primaries, sizeof(srcInfo.primaries)) ||
            memcmp(&srcInfo.curve, &dstInfo.curve, sizeof(srcInfo.curve)) ||
            (srcInfo.luminance != dstInfo.luminance) ||
            (dstProfile != NULL) ||
            (haldImage != NULL)
            ) ? clTrue : clFalse;

        needsResize = (
            ((dstInfo.width != srcInfo.width) || (dstInfo.height != srcInfo.height))
            ) ? clTrue : clFalse;

        needsFloatRedepth = ( // LittleCMS can only directly convert from/to 8 or 16 bit formats
            ((srcInfo.depth != 8) && (srcInfo.depth != 16)) ||
            ((dstInfo.depth != 8) && (dstInfo.depth != 16))
            ) ? clTrue : clFalse;

        needsLinearPixels = (
            needsColorConversion ||
            needsResize ||    // Resizing modifies/replaces linearFloatsPixels, so srcFloatsPixels isn't sufficient
            params->autoGrade // If autograding, we need linear pixels even if we don't need to color convert afterwards
            ) ? clTrue : clFalse;

        needsSrcFloats = (
            needsLinearPixels || // linearFloatsPixels is converted from srcFloatsPixels
            needsFloatRedepth
            ) ? clTrue : clFalse;

        if (needsSrcFloats) {
            clContextLog(C, "convert", 0, "Creating source floats...");
            timerStart(&t);
            srcFloatsPixels = clAllocate(4 * sizeof(float) * pixelCount);
            clPixelMathUNormToFloat(C, srcImage->pixels, srcImage->depth, srcFloatsPixels, pixelCount);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }

        if (needsLinearPixels) {
            cmsHTRANSFORM toLinear;

            clProfileCurve gamma1;
            gamma1.type = CL_PCT_GAMMA;
            gamma1.gamma = 1.0f;
            linearFloatsProfile = clProfileCreate(C, &dstInfo.primaries, &gamma1, 0, NULL);
            toLinear = cmsCreateTransformTHR(C->lcms, srcImage->profile->handle, TYPE_RGBA_FLT, linearFloatsProfile->handle, TYPE_RGBA_FLT, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

            clContextLog(C, "convert", 0, "Calculating linear pixels...");
            timerStart(&t);
            linearFloatsPixels = clAllocate(4 * sizeof(float) * pixelCount);
            doMultithreadedTransform(C, params->jobs, toLinear, (uint8_t *)srcFloatsPixels, 4 * sizeof(float), (uint8_t *)linearFloatsPixels, 4 * sizeof(float), pixelCount);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

            cmsDeleteTransform(toLinear);
        }
    }

    // -----------------------------------------------------------------------
    // Color grading

    if (params->autoGrade) {
        COLORIST_ASSERT(dstProfile == NULL);
        COLORIST_ASSERT(linearFloatsPixels != NULL);

        clContextLog(C, "grading", 0, "Color grading...");
        timerStart(&t);
        dstInfo.curve.type = CL_PCT_GAMMA;
        clPixelMathColorGrade(C, params->jobs, linearFloatsProfile, linearFloatsPixels, pixelCount, srcInfo.width, srcInfo.luminance, dstInfo.depth, &dstInfo.luminance, &dstInfo.curve.gamma, C->verbose);
        clContextLog(C, "grading", 0, "Using maxLum: %d, gamma: %g", dstInfo.luminance, dstInfo.curve.gamma);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Profile params validation & profile creation

    // Create the destination profile, or clone the source one
    if (dstProfile == NULL) {
        if (
            memcmp(&srcInfo.primaries, &dstInfo.primaries, sizeof(srcInfo.primaries)) || // Custom primaries
            memcmp(&srcInfo.curve, &dstInfo.curve, sizeof(srcInfo.curve)) ||             // Custom curve
            (srcInfo.luminance != dstInfo.luminance) ||                                  // Custom luminance
            (params->description) ||                                                     // Custom description
            (params->copyright)                                                          // custom copyright
            )
        {
            char * dstDescription = NULL;

            // Primaries
            if ((dstInfo.primaries.red[0] <= 0.0f) || (dstInfo.primaries.red[1] <= 0.0f) ||
                (dstInfo.primaries.green[0] <= 0.0f) || (dstInfo.primaries.green[1] <= 0.0f) ||
                (dstInfo.primaries.blue[0] <= 0.0f) || (dstInfo.primaries.blue[1] <= 0.0f) ||
                (dstInfo.primaries.white[0] <= 0.0f) || (dstInfo.primaries.white[1] <= 0.0f))
            {
                clContextLogError(C, "Can't create destination profile, destination primaries are invalid");
                FAIL();
            }

            // Curve
            if (dstInfo.curve.type != CL_PCT_GAMMA) {
                // TODO: Support/pass-through any source curve
                clContextLogError(C, "Can't create destination profile, tone curve cannot be created as it isn't just a simple gamma curve. Try choosing a new curve (-g) or autograding (-a)");
                FAIL();
            }
            if (dstInfo.curve.gamma <= 0.0f) {
                // TODO: Support/pass-through any source curve
                clContextLogError(C, "Can't create destination profile, gamma(%g) is invalid", dstInfo.curve.gamma);
                FAIL();
            }

            if (dstInfo.luminance == 0) {
                clContextLogError(C, "Can't create destination profile, luminance(%d) is invalid", dstInfo.luminance);
                FAIL();
            }

            // Description
            if (params->description) {
                dstDescription = clContextStrdup(C, params->description);
            } else {
                dstDescription = clGenerateDescription(C, &dstInfo.primaries, &dstInfo.curve, dstInfo.luminance);
            }

            clContextLog(C, "profile", 0, "Creating new destination ICC profile: \"%s\"", dstDescription);
            dstProfile = clProfileCreate(C, &dstInfo.primaries, &dstInfo.curve, dstInfo.luminance, dstDescription);
            clFree(dstDescription);

            // Copyright
            if (params->copyright) {
                clContextLog(C, "profile", 1, "Setting copyright: \"%s\"", params->copyright);
                clProfileSetMLU(C, dstProfile, "cprt", "en", "US", params->copyright);
            }
        } else {
            // just clone the source one
            clContextLog(C, "profile", 0, "Using unmodified source ICC profile: \"%s\"", srcImage->profile->description);
            dstProfile = clProfileClone(C, srcImage->profile);
        }
    }

    // -----------------------------------------------------------------------
    // Resize, if necessary

    if (needsResize) {
        int resizedPixelCount = dstInfo.width * dstInfo.height;
        float * resizedPixels = clAllocate(4 * sizeof(float) * resizedPixelCount);
        clContextLog(C, "resize", 0, "Resizing %dx%d -> [filter:%s] -> %dx%d", srcInfo.width, srcInfo.height, clFilterToString(C, params->resizeFilter), dstInfo.width, dstInfo.height);
        timerStart(&t);
        clPixelMathResize(C, srcImage->width, srcImage->height, linearFloatsPixels, dstInfo.width, dstInfo.height, resizedPixels, params->resizeFilter);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        clFree(linearFloatsPixels);
        linearFloatsPixels = resizedPixels;
        pixelCount = resizedPixelCount;
    }

    // -----------------------------------------------------------------------
    // Create destination image

    dstImage = clImageCreate(C, dstInfo.width, dstInfo.height, dstInfo.depth, dstProfile);

    // Show image details
    clContextLog(C, "details", 0, "Source:");
    clImageDebugDump(C, srcImage, 0, 0, 0, 0, 1);
    clContextLog(C, "details", 0, "Destination:");
    clImageDebugDump(C, dstImage, 0, 0, 0, 0, 1);

    // -----------------------------------------------------------------------
    // Image conversion

    // At this point, there are three possible ways to convert, based on which sets of pixels we have:
    // * If we have linearFloatsPixels, we chose to do all of the heavy lifting
    // * If we only have srcFloatsPixels, we can just repack floats (typically when only depth changes or an 'interesting' depth is used)
    // * If we haven't made any intermediate pixel sets, we can just have LittleCMS do everything (direct conversion)

    if (linearFloatsPixels) {
        // Do everything!

        cmsHTRANSFORM fromLinear = cmsCreateTransformTHR(C->lcms, linearFloatsProfile->handle, TYPE_RGBA_FLT, dstImage->profile->handle, TYPE_RGBA_FLT, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
        float * dstFloatsPixels; // final values in floating point, manually created to avoid cms eval'ing on a 16-bit basis for floats (yuck)

        if (srcInfo.luminance != dstInfo.luminance) {
            float luminanceScale = (float)srcInfo.luminance / (float)dstInfo.luminance;
            clBool tonemap;
            if (params->autoGrade) {
                // autoGrade ensures we're never scaling a pixel lower than the brighest
                // pixel in the source image, tonemapping is unnecessary.
                tonemap = clFalse;
            } else {
                // tonemap if we're scaling from a larger luminance range to a smaller range
                tonemap = (luminanceScale > 1.0f) ? clTrue : clFalse;
            }
            if (params->tonemap != CL_TONEMAP_AUTO) {
                tonemap = (params->tonemap == CL_TONEMAP_ON) ? clTrue : clFalse;
            }

            clContextLog(C, "luminance", 0, "Scaling luminance (%gx, %s)...", luminanceScale, tonemap ? "tonemap" : "clip");
            timerStart(&t);
            clPixelMathScaleLuminance(C, linearFloatsPixels, pixelCount, luminanceScale, tonemap);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }

        clContextLog(C, "convert", 0, "Performing color conversion...");
        timerStart(&t);
        dstFloatsPixels = clAllocate(4 * sizeof(float) * pixelCount);
        doMultithreadedTransform(C, params->jobs, fromLinear, (uint8_t *)linearFloatsPixels, 4 * sizeof(float), (uint8_t *)dstFloatsPixels, 4 * sizeof(float), pixelCount);
        cmsDeleteTransform(fromLinear);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

        if (haldImage) {
            int i;
            int haldDataCount = haldImage->width * haldImage->height;
            float * haldSrcFloats;
            float * haldData;

            clContextLog(C, "hald", 0, "Performing Hald CLUT postprocessing...");
            timerStart(&t);

            haldData = clAllocate(4 * sizeof(float) * haldDataCount);
            clPixelMathUNormToFloat(C, haldImage->pixels, haldImage->depth, haldData, haldDataCount);

            haldSrcFloats = dstFloatsPixels;
            dstFloatsPixels = clAllocate(4 * sizeof(float) * pixelCount);

            for (i = 0; i < pixelCount; ++i) {
                clPixelMathHaldCLUTLookup(C, haldData, haldDims, &haldSrcFloats[i * 4], &dstFloatsPixels[i * 4]);
            }

            clFree(haldData);
            clFree(haldSrcFloats);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }
        clPixelMathFloatToUNorm(C, dstFloatsPixels, dstImage->pixels, dstImage->depth, pixelCount);
        clFree(dstFloatsPixels);
    } else if (srcFloatsPixels) {
        // Just repackage source floats (typically just a re-depth)

        clContextLog(C, "convert", 0, "Packing final pixels from source floats...");
        timerStart(&t);
        clPixelMathFloatToUNorm(C, srcFloatsPixels, dstImage->pixels, dstImage->depth, pixelCount);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    } else {
        // Let LittleCMS directly convert
        cmsHTRANSFORM directTransform;
        cmsUInt32Number srcFormat = (srcInfo.depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;
        cmsUInt32Number dstFormat = (dstInfo.depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;

        COLORIST_ASSERT((srcInfo.depth == 8) || (srcInfo.depth == 16));
        COLORIST_ASSERT((dstInfo.depth == 8) || (dstInfo.depth == 16));

        clContextLog(C, "convert", 0, "Converting directly...");
        timerStart(&t);
        directTransform = cmsCreateTransformTHR(C->lcms, srcImage->profile->handle, srcFormat, dstImage->profile->handle, dstFormat, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
        doMultithreadedTransform(C, params->jobs, directTransform, srcImage->pixels, (srcImage->depth == 16) ? 8 : 4, dstImage->pixels, (dstImage->depth == 16) ? 8 : 4, dstImage->width * dstImage->height);
        cmsDeleteTransform(directTransform);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Cleanup

convertCleanup:

    if (dstProfile)
        clProfileDestroy(C, dstProfile);
    if (linearFloatsProfile)
        clProfileDestroy(C, linearFloatsProfile);
    if (srcFloatsPixels)
        clFree(srcFloatsPixels);
    if (linearFloatsPixels)
        clFree(linearFloatsPixels);
    if (haldImage)
        clImageDestroy(C, haldImage);

    if (!result) {
        if (dstImage) {
            clImageDestroy(C, dstImage);
            dstImage = NULL;
        }
    }
    return dstImage;
}

typedef struct clTransformTask
{
    cmsHTRANSFORM transform;
    void * inPixels;
    void * outPixels;
    int pixelCount;
} clTransformTask;

static void transformTaskFunc(clTransformTask * info)
{
    cmsDoTransform(info->transform, info->inPixels, info->outPixels, info->pixelCount);
}

void doMultithreadedTransform(clContext * C, int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    if (taskCount > pixelCount) {
        // This is a dumb corner case I'm not too worried about.
        taskCount = pixelCount;
    }

    if (taskCount == 1) {
        // Don't bother making any new threads
        cmsDoTransform(transform, srcPixels, dstPixels, pixelCount);
    } else {
        int pixelsPerTask = pixelCount / taskCount;
        int lastTaskPixelCount = pixelCount - (pixelsPerTask * (taskCount - 1));
        clTask ** tasks;
        clTransformTask * infos;
        int i;

        clContextLog(C, "convert", 1, "Using %d thread%s to pixel transform.", taskCount, (taskCount == 1) ? "" : "s");

        tasks = clAllocate(taskCount * sizeof(clTask *));
        infos = clAllocate(taskCount * sizeof(clTransformTask));
        for (i = 0; i < taskCount; ++i) {
            infos[i].transform = transform;
            infos[i].inPixels = &srcPixels[i * pixelsPerTask * srcPixelBytes];
            infos[i].outPixels = &dstPixels[i * pixelsPerTask * dstPixelBytes];
            infos[i].pixelCount = (i == (taskCount - 1)) ? lastTaskPixelCount : pixelsPerTask;
            tasks[i] = clTaskCreate(C, (clTaskFunc)transformTaskFunc, &infos[i]);
        }

        for (i = 0; i < taskCount; ++i) {
            clTaskDestroy(C, tasks[i]);
        }

        clFree(tasks);
        clFree(infos);
    }
}
