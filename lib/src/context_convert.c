// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/image.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/task.h"

#include <string.h>

#define FAIL() { returnCode = 1; goto convertCleanup; }

static int fileSize(const char * filename);
static void doMultithreadedTransform(clContext * C, int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount);

int clContextConvert(clContext * C)
{
    Timer overall, t;
    int returnCode = 0;

    clImage * srcImage = NULL;
    float srcGamma = 0.0f;
    int srcLuminance = 0;

    clProfilePrimaries dstPrimaries;
    float dstGamma = 0.0f;
    int dstLuminance = 0;
    int dstDepth = 0;
    clProfile * dstProfile = NULL;
    clImage * dstImage = NULL;

    // Variables used in luminance scaling
    clProfile * dstLinear = NULL;
    int linearPixelsCount = 0;
    float * linearPixels = NULL;
    float luminanceScale = 0.0f;
    clBool tonemap = clFalse;

    clFormat outputFileFormat = C->format;
    if (outputFileFormat == CL_FORMAT_AUTO)
        outputFileFormat = clFormatDetect(C, C->outputFilename);
    if (outputFileFormat == CL_FORMAT_ERROR) {
        clContextLogError(C, "Unknown output file format: %s", C->outputFilename);
        FAIL();
    }

    clContextLog(C, "action", 0, "Convert: %s -> %s", C->inputFilename, C->outputFilename);
    timerStart(&overall);

    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    timerStart(&t);
    srcImage = clContextRead(C, C->inputFilename, NULL);
    if (srcImage == NULL) {
        return 1;
    }
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    // Parse source image and args for early pipeline decisions
    {
        clProfileCurve curve;
        clProfileQuery(C, srcImage->profile, &dstPrimaries, &curve, &srcLuminance);

        // Primaries
        if (C->primaries[0] > 0.0f) {
            dstPrimaries.red[0] = C->primaries[0];
            dstPrimaries.red[1] = C->primaries[1];
            dstPrimaries.green[0] = C->primaries[2];
            dstPrimaries.green[1] = C->primaries[3];
            dstPrimaries.blue[0] = C->primaries[4];
            dstPrimaries.blue[1] = C->primaries[5];
            dstPrimaries.white[0] = C->primaries[6];
            dstPrimaries.white[1] = C->primaries[7];
        }

        // Luminance
        srcLuminance = (srcLuminance != 0) ? srcLuminance : COLORIST_DEFAULT_LUMINANCE;
        if (C->luminance < 0) {
            dstLuminance = srcLuminance;
        } else if (C->luminance != 0) {
            dstLuminance = C->luminance;
        }

        // Gamma
        if (curve.type == CL_PCT_GAMMA) {
            srcGamma = curve.gamma;
        }
        if (C->gamma < 0.0f) {
            dstGamma = srcGamma;
        } else if (C->gamma > 0.0f) {
            dstGamma = C->gamma;
        }

        // Depth
        dstDepth = C->bpp;
        if (dstDepth == 0) {
            dstDepth = srcImage->depth;
        }
        if ((dstDepth != 8) && (outputFileFormat == CL_FORMAT_JPG)) {
            clContextLog(C, "validate", 0, "Forcing output to 8-bit (JPEG limitations)");
            dstDepth = 8;
        }

        if (!C->autoGrade) {
            if (dstGamma == 0.0f) {
                dstGamma = srcGamma;
            }
            if (dstLuminance == 0) {
                dstLuminance = srcLuminance;
            }
        }
    }

    // Create intermediate 1.0 gamma float32 pixel array if we're going to need it later.
    if ((outputFileFormat != CL_FORMAT_ICC) && ((srcLuminance != dstLuminance))) {
        cmsHTRANSFORM toLinear;
        float * srcFloats; // original values in floating point, manually created to avoid cms eval'ing on a 16-bit basis for floats (yuck)

        clProfileCurve gamma1;
        gamma1.type = CL_PCT_GAMMA;
        gamma1.gamma = 1.0f;
        dstLinear = clProfileCreate(C, &dstPrimaries, &gamma1, 0, NULL);

        linearPixelsCount = srcImage->width * srcImage->height;
        linearPixels = clAllocate(4 * sizeof(float) * linearPixelsCount);
        toLinear = cmsCreateTransformTHR(C->lcms, srcImage->profile->handle, TYPE_RGBA_FLT, dstLinear->handle, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

        clContextLog(C, "convert", 0, "Calculating linear pixels...");
        timerStart(&t);
        srcFloats = clAllocate(4 * sizeof(float) * linearPixelsCount);
        clPixelMathUNormToFloat(C, srcImage->pixels, srcImage->depth, srcFloats, linearPixelsCount);
        doMultithreadedTransform(C, C->jobs, toLinear, (uint8_t *)srcFloats, 4 * sizeof(float), (uint8_t *)linearPixels, 4 * sizeof(float), linearPixelsCount);
        cmsDeleteTransform(toLinear);
        clFree(srcFloats);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    if (C->autoGrade) {
        clContextLog(C, "grading", 0, "Color grading...");
        timerStart(&t);
        clPixelMathColorGrade(C, C->jobs, linearPixels, linearPixelsCount, srcLuminance, dstDepth, &dstLuminance, &dstGamma, C->verbose);
        clContextLog(C, "grading", 0, "Using maxLum: %d, gamma: %g", dstLuminance, dstGamma);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // If we survive arg parsing and autoGrade mode and still don't have a reasonable luminance and gamma, bail out.
    if ((dstLuminance == 0) || (dstGamma == 0.0f)) {
        clContextLogError(C, "Can't create destination profile, luminance(%d) and/or gamma(%g) values are invalid", dstLuminance, dstGamma);
        FAIL();
    }

    // Calculate luminance scale and tonemapping
    COLORIST_ASSERT(srcLuminance > 0);
    COLORIST_ASSERT(dstLuminance > 0);
    luminanceScale = (float)srcLuminance / (float)dstLuminance;
    if (C->autoGrade) {
        // autoGrade ensures we're never scaling a pixel lower than the brighest
        // pixel in the source image, tonemapping is unnecessary.
        tonemap = clFalse;
    } else {
        // tonemap if we're scaling from a larger luminance range to a smaller range
        tonemap = (luminanceScale > 1.0f) ? clTrue : clFalse;
    }
    if (C->tonemap != CL_TONEMAP_AUTO) {
        tonemap = (C->tonemap == CL_TONEMAP_ON) ? clTrue : clFalse;
    }

    // Create the destination profile, or clone the source one
    {
        if (
            (C->primaries[0] > 0.0f) ||       // Custom primaries
            (srcGamma != dstGamma) ||         // Custom gamma
            (srcLuminance != dstLuminance) || // Custom luminance
            (C->description) ||               // Custom description
            (C->copyright)                    // custom copyright
            )
        {
            clProfileCurve dstCurve;
            char * dstDescription = NULL;

            // Primaries
            if ((dstPrimaries.red[0] <= 0.0f) || (dstPrimaries.red[1] <= 0.0f) ||
                (dstPrimaries.green[0] <= 0.0f) || (dstPrimaries.green[1] <= 0.0f) ||
                (dstPrimaries.blue[0] <= 0.0f) || (dstPrimaries.blue[1] <= 0.0f) ||
                (dstPrimaries.white[0] <= 0.0f) || (dstPrimaries.white[1] <= 0.0f))
            {
                clContextLogError(C, "Can't create destination profile, destination primaries are invalid");
                FAIL();
            }

            // Gamma
            if (dstGamma == 0.0f) {
                // TODO: Support/pass-through any source curve
                clContextLogError(C, "Can't create destination profile, source profile's curve cannot be re-created as it isn't just a simple gamma curve");
                FAIL();
            }
            dstCurve.type = CL_PCT_GAMMA;
            dstCurve.gamma = dstGamma;

            // Luminance
            if (dstLuminance == 0) {
                dstLuminance = srcLuminance;
            }

            // Description
            if (C->description) {
                dstDescription = clContextStrdup(C, C->description);
            } else {
                dstDescription = clGenerateDescription(C, &dstPrimaries, &dstCurve, dstLuminance);
            }

            clContextLog(C, "profile", 0, "Creating new destination ICC profile: \"%s\"", dstDescription);
            dstProfile = clProfileCreate(C, &dstPrimaries, &dstCurve, dstLuminance, dstDescription);
            clFree(dstDescription);

            // Copyright
            if (C->copyright) {
                clContextLog(C, "profile", 1, "Setting copyright: \"%s\"", C->copyright);
                clProfileSetMLU(C, dstProfile, "cprt", "en", "US", C->copyright);
            }
        } else {
            // just clone the source one
            clContextLog(C, "profile", 0, "Using source ICC profile: \"%s\"", srcImage->profile->description);
            dstProfile = clProfileClone(C, srcImage->profile);
        }
    }

    if (outputFileFormat == CL_FORMAT_ICC) {
        // Just dump out the profile to disk and bail out

        clContextLog(C, "encode", 0, "Writing ICC: %s", C->outputFilename);
        clProfileDebugDump(C, dstProfile, 0);

        if (!clProfileWrite(C, dstProfile, C->outputFilename)) {
            FAIL();
        }
        goto convertCleanup;
    }

    // Create dstImage
    dstImage = clImageCreate(C, srcImage->width, srcImage->height, dstDepth, dstProfile);

    // Show image details
    {
        clContextLog(C, "details", 0, "Source:");
        clImageDebugDump(C, srcImage, 0, 0, 0, 0, 1);
        clContextLog(C, "details", 0, "Destination:");
        clImageDebugDump(C, dstImage, 0, 0, 0, 0, 1);
    }

    // Convert srcImage -> dstImage
    {
        if (linearPixels == NULL) {
            cmsHTRANSFORM directTransform;
            cmsUInt32Number srcFormat = (srcImage->depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;
            cmsUInt32Number dstFormat = (dstImage->depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;

            clContextLog(C, "convert", 0, "Converting directly...");
            timerStart(&t);
            directTransform = cmsCreateTransformTHR(C->lcms, srcImage->profile->handle, srcFormat, dstImage->profile->handle, dstFormat, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
            doMultithreadedTransform(C, C->jobs, directTransform, srcImage->pixels, (srcImage->depth == 16) ? 8 : 4, dstImage->pixels, (dstImage->depth == 16) ? 8 : 4, dstImage->width * dstImage->height);
            cmsDeleteTransform(directTransform);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        } else {
            cmsHTRANSFORM fromLinear = cmsCreateTransformTHR(C->lcms, dstLinear->handle, TYPE_RGBA_FLT, dstImage->profile->handle, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
            float * dstFloats; // final values in floating point, manually created to avoid cms eval'ing on a 16-bit basis for floats (yuck)

            clContextLog(C, "luminance", 0, "Scaling luminance (%s)...", tonemap ? "tonemap" : "clip");
            timerStart(&t);
            clPixelMathScaleLuminance(C, linearPixels, linearPixelsCount, luminanceScale, tonemap);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

            clContextLog(C, "convert", 0, "Calculating final pixel values...");
            timerStart(&t);
            dstFloats = clAllocate(4 * sizeof(float) * linearPixelsCount);
            doMultithreadedTransform(C, C->jobs, fromLinear, (uint8_t *)linearPixels, 4 * sizeof(float), (uint8_t *)dstFloats, 4 * sizeof(float), linearPixelsCount);
            cmsDeleteTransform(fromLinear);
            clPixelMathFloatToUNorm(C, dstFloats, dstImage->pixels, dstImage->depth, linearPixelsCount);
            clFree(dstFloats);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }
    }

    switch (outputFileFormat) {
        case CL_FORMAT_JP2:
            clContextLog(C, "encode", 0, "Writing JP2 [%s:%d]: %s", (C->rate) ? "R" : "Q", (C->rate) ? C->rate : C->quality, C->outputFilename);
            break;
        case CL_FORMAT_JPG:
            clContextLog(C, "encode", 0, "Writing JPG [Q:%d]: %s", C->quality, C->outputFilename);
            break;
        default:
            clContextLog(C, "encode", 0, "Writing: %s", C->outputFilename);
            break;
    }
    timerStart(&t);
    if (!clContextWrite(C, dstImage, C->outputFilename, outputFileFormat, C->quality, C->rate)) {
        FAIL();
    }
    clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

convertCleanup:
    if (srcImage)
        clImageDestroy(C, srcImage);
    if (dstProfile)
        clProfileDestroy(C, dstProfile);
    if (dstImage)
        clImageDestroy(C, dstImage);
    if (dstLinear)
        clProfileDestroy(C, dstLinear);
    if (linearPixels)
        clFree(linearPixels);

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "Conversion complete.");
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
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

static void doMultithreadedTransform(clContext * C, int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
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
