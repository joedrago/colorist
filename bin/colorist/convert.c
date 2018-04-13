// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include "colorist/pixelmath.h"
#include "colorist/task.h"

#include <string.h>

#define FAIL() { returnCode = 1; goto convertCleanup; }

static int fileSize(const char * filename);
static void doMultithreadedTransform(int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount);

int actionConvert(Args * args)
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

    Format outputFileFormat = args->format;
    if (outputFileFormat == FORMAT_AUTO)
        outputFileFormat = detectFormat(args->outputFilename);
    if (outputFileFormat == FORMAT_ERROR) {
        clLogError("Unknown output file format: %s", args->outputFilename);
        FAIL();
    }

    clLog("action", 0, "Convert: %s -> %s", args->inputFilename, args->outputFilename);
    timerStart(&overall);

    clLog("decode", 0, "Reading: %s (%d bytes)", args->inputFilename, clFileSize(args->inputFilename));
    timerStart(&t);
    srcImage = readImage(args->inputFilename, NULL);
    if (srcImage == NULL) {
        return 1;
    }
    clLog("timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    // Parse source image and args for early pipeline decisions
    {
        clProfileCurve curve;
        clProfileQuery(srcImage->profile, &dstPrimaries, &curve, &srcLuminance);

        // Primaries
        if (args->primaries[0] > 0.0f) {
            dstPrimaries.red[0] = args->primaries[0];
            dstPrimaries.red[1] = args->primaries[1];
            dstPrimaries.green[0] = args->primaries[2];
            dstPrimaries.green[1] = args->primaries[3];
            dstPrimaries.blue[0] = args->primaries[4];
            dstPrimaries.blue[1] = args->primaries[5];
            dstPrimaries.white[0] = args->primaries[6];
            dstPrimaries.white[1] = args->primaries[7];
        }

        // Luminance
        srcLuminance = (srcLuminance != 0) ? srcLuminance : COLORIST_DEFAULT_LUMINANCE;
        if (args->luminance < 0) {
            dstLuminance = srcLuminance;
        } else if (args->luminance != 0) {
            dstLuminance = args->luminance;
        }

        // Gamma
        if (curve.type == CL_PCT_GAMMA) {
            srcGamma = curve.gamma;
        }
        if (args->gamma < 0.0f) {
            dstGamma = srcGamma;
        } else if (args->gamma > 0.0f) {
            dstGamma = args->gamma;
        }

        // Depth
        dstDepth = args->bpp;
        if (dstDepth == 0) {
            dstDepth = srcImage->depth;
        }
        if ((dstDepth != 8) && (outputFileFormat == FORMAT_JPG)) {
            clLog("validate", 0, "Forcing output to 8-bit (JPEG limitations)");
            dstDepth = 8;
        }

        if (!args->autoGrade) {
            if (dstGamma == 0.0f) {
                dstGamma = srcGamma;
            }
            if (dstLuminance == 0) {
                dstLuminance = srcLuminance;
            }
        }
    }

    // Create intermediate 1.0 gamma float32 pixel array if we're going to need it later.
    if ((outputFileFormat != FORMAT_ICC) && ((srcLuminance != dstLuminance))) {
        cmsHTRANSFORM toLinear;
        float * srcFloats; // original values in floating point, manually created to avoid cms eval'ing on a 16-bit basis for floats (yuck)

        clProfileCurve gamma1;
        gamma1.type = CL_PCT_GAMMA;
        gamma1.gamma = 1.0f;
        dstLinear = clProfileCreate(&dstPrimaries, &gamma1, 0, NULL);

        linearPixelsCount = srcImage->width * srcImage->height;
        linearPixels = malloc(4 * sizeof(float) * linearPixelsCount);
        toLinear = cmsCreateTransform(srcImage->profile->handle, TYPE_RGBA_FLT, dstLinear->handle, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);

        clLog("convert", 0, "Calculating linear pixels...");
        timerStart(&t);
        srcFloats = malloc(4 * sizeof(float) * linearPixelsCount);
        clPixelMathUNormToFloat(srcImage->pixels, srcImage->depth, srcFloats, linearPixelsCount);
        doMultithreadedTransform(args->jobs, toLinear, (uint8_t *)srcFloats, 4 * sizeof(float), (uint8_t *)linearPixels, 4 * sizeof(float), linearPixelsCount);
        free(srcFloats);
        clLog("timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    if (args->autoGrade) {
        clLog("grading", 0, "Color grading...");
        timerStart(&t);
        clPixelMathColorGrade(args->jobs, linearPixels, linearPixelsCount, srcLuminance, dstDepth, &dstLuminance, &dstGamma, args->verbose);
        clLog("grading", 0, "Using maxLum: %d, gamma: %g", dstLuminance, dstGamma);
        clLog("timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // If we survive arg parsing and autoGrade mode and still don't have a reasonable luminance and gamma, bail out.
    if ((dstLuminance == 0) || (dstGamma == 0.0f)) {
        clLogError("Can't create destination profile, luminance(%d) and/or gamma(%g) values are invalid", dstLuminance, dstGamma);
        FAIL();
    }

    // Calculate luminance scale and tonemapping
    COLORIST_ASSERT(srcLuminance > 0);
    COLORIST_ASSERT(dstLuminance > 0);
    luminanceScale = (float)srcLuminance / (float)dstLuminance;
    if (args->autoGrade) {
        // autoGrade ensures we're never scaling a pixel lower than the brighest
        // pixel in the source image, tonemapping is unnecessary.
        tonemap = clFalse;
    } else {
        // tonemap if we're scaling from a larger luminance range to a smaller range
        tonemap = (luminanceScale > 1.0f) ? clTrue : clFalse;
    }
    if (args->tonemap != TONEMAP_AUTO) {
        tonemap = (args->tonemap == TONEMAP_ON) ? clTrue : clFalse;
    }

    // Create the destination profile, or clone the source one
    {
        if (
            (args->primaries[0] > 0.0f) ||    // Custom primaries
            (srcGamma != dstGamma) ||         // Custom gamma
            (srcLuminance != dstLuminance) || // Custom luminance
            (args->description) ||            // Custom description
            (args->copyright)                 // custom copyright
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
                clLogError("Can't create destination profile, destination primaries are invalid");
                FAIL();
            }

            // Gamma
            if (dstGamma == 0.0f) {
                // TODO: Support/pass-through any source curve
                clLogError("Can't create destination profile, source profile's curve cannot be re-created as it isn't just a simple gamma curve");
                FAIL();
            }
            dstCurve.type = CL_PCT_GAMMA;
            dstCurve.gamma = dstGamma;

            // Luminance
            if (dstLuminance == 0) {
                dstLuminance = srcLuminance;
            }

            // Description
            if (args->description) {
                dstDescription = strdup(args->description);
            } else {
                dstDescription = generateDescription(&dstPrimaries, &dstCurve, dstLuminance);
            }

            clLog("profile", 0, "Creating new destination ICC profile: \"%s\"", dstDescription);
            dstProfile = clProfileCreate(&dstPrimaries, &dstCurve, dstLuminance, dstDescription);
            free(dstDescription);

            // Copyright
            if (args->copyright) {
                clLog("profile", 1, "Setting copyright: \"%s\"", args->copyright);
                clProfileSetMLU(dstProfile, "cprt", "en", "US", args->copyright);
            }
        } else {
            // just clone the source one
            clLog("profile", 0, "Using source ICC profile: \"%s\"", srcImage->profile->description);
            dstProfile = clProfileClone(srcImage->profile);
        }
    }

    if (outputFileFormat == FORMAT_ICC) {
        // Just dump out the profile to disk and bail out

        clLog("encode", 0, "Writing ICC: %s", args->outputFilename);
        clProfileDebugDump(dstProfile, 0);

        if (!clProfileWrite(dstProfile, args->outputFilename)) {
            FAIL();
        }
        goto convertCleanup;
    }

    // Create dstImage
    dstImage = clImageCreate(srcImage->width, srcImage->height, dstDepth, dstProfile);

    // Show image details
    {
        clLog("details", 0, "Source:");
        clImageDebugDump(srcImage, 0, 0, 0, 0, 1);
        clLog("details", 0, "Destination:");
        clImageDebugDump(dstImage, 0, 0, 0, 0, 1);
    }

    // Convert srcImage -> dstImage
    {
        if (linearPixels == NULL) {
            cmsHTRANSFORM directTransform;
            cmsUInt32Number srcFormat = (srcImage->depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;
            cmsUInt32Number dstFormat = (dstImage->depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;

            clLog("convert", 0, "Converting directly...");
            timerStart(&t);
            directTransform = cmsCreateTransform(srcImage->profile->handle, srcFormat, dstImage->profile->handle, dstFormat, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
            doMultithreadedTransform(args->jobs, directTransform, srcImage->pixels, (srcImage->depth == 16) ? 8 : 4, dstImage->pixels, (dstImage->depth == 16) ? 8 : 4, dstImage->width * dstImage->height);
            clLog("timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        } else {
            cmsHTRANSFORM fromLinear = cmsCreateTransform(dstLinear->handle, TYPE_RGBA_FLT, dstImage->profile->handle, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
            float * dstFloats; // final values in floating point, manually created to avoid cms eval'ing on a 16-bit basis for floats (yuck)

            clLog("luminance", 0, "Scaling luminance (%s)...", tonemap ? "tonemap" : "clip");
            timerStart(&t);
            clPixelMathScaleLuminance(linearPixels, linearPixelsCount, luminanceScale, tonemap);
            clLog("timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

            clLog("convert", 0, "Calculating final pixel values...");
            timerStart(&t);
            dstFloats = malloc(4 * sizeof(float) * linearPixelsCount);
            doMultithreadedTransform(args->jobs, fromLinear, (uint8_t *)linearPixels, 4 * sizeof(float), (uint8_t *)dstFloats, 4 * sizeof(float), linearPixelsCount);
            clPixelMathFloatToUNorm(dstFloats, dstImage->pixels, dstImage->depth, linearPixelsCount);
            free(dstFloats);
            clLog("timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }
    }

    switch (outputFileFormat) {
        case FORMAT_JP2:
            clLog("encode", 0, "Writing JP2 [%s:%d]: %s", (args->rate) ? "R" : "Q", (args->rate) ? args->rate : args->quality, args->outputFilename);
            break;
        case FORMAT_JPG:
            clLog("encode", 0, "Writing JPG [Q:%d]: %s", args->quality, args->outputFilename);
            break;
        default:
            clLog("encode", 0, "Writing: %s", args->outputFilename);
            break;
    }
    timerStart(&t);
    if (!writeImage(dstImage, args->outputFilename, outputFileFormat, args->quality, args->rate)) {
        FAIL();
    }
    clLog("encode", 1, "Wrote %d bytes.", clFileSize(args->outputFilename));
    clLog("timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

convertCleanup:
    if (srcImage)
        clImageDestroy(srcImage);
    if (dstProfile)
        clProfileDestroy(dstProfile);
    if (dstImage)
        clImageDestroy(dstImage);
    if (dstLinear)
        clProfileDestroy(dstLinear);
    if (linearPixels)
        free(linearPixels);

    if (returnCode == 0) {
        clLog("action", 0, "Conversion complete.");
        clLog("timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
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

static void doMultithreadedTransform(int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
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

        clLog("convert", 1, "Using %d thread%s to pixel transform.", taskCount, (taskCount == 1) ? "" : "s");

        tasks = calloc(taskCount, sizeof(clTask *));
        infos = calloc(taskCount, sizeof(clTransformTask));
        for (i = 0; i < taskCount; ++i) {
            infos[i].transform = transform;
            infos[i].inPixels = &srcPixels[i * pixelsPerTask * srcPixelBytes];
            infos[i].outPixels = &dstPixels[i * pixelsPerTask * dstPixelBytes];
            infos[i].pixelCount = (i == (taskCount - 1)) ? lastTaskPixelCount : pixelsPerTask;
            tasks[i] = clTaskCreate((clTaskFunc)transformTaskFunc, &infos[i]);
        }

        for (i = 0; i < taskCount; ++i) {
            clTaskDestroy(tasks[i]);
        }

        free(tasks);
        free(infos);
    }
}
