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

#define FAIL()          \
    {                   \
        returnCode = 1; \
        goto cleanup;   \
    }

struct ImageInfo
{
    int width;
    int height;
    int depth;

    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance;
};

int clContextRun(clContext * C, struct cJSON * jsonOutput)
{
    (void)jsonOutput;

    Timer overall, t;
    int returnCode = 0;
    clImage * image = NULL;
    clProfile * dstProfile = NULL;

    // Information about the src&dst images, used to make all decisions
    struct ImageInfo srcInfo;
    struct ImageInfo dstInfo;

    // Hald CLUT
    clImage * haldImage = NULL;
    int haldDims = 0;

    // -----------------------------------------------------------------------
    // Clone the incoming params in case adjustments are necessary

    clConversionParams params;
    memcpy(&params, &C->params, sizeof(params));

    if (C->outputFilename) {
        if (!params.formatName)
            params.formatName = clFormatDetect(C, C->outputFilename);
        if (!params.formatName) {
            clContextLogError(C, "Unknown output file format: %s", C->outputFilename);
            FAIL();
        }
    }

    // -----------------------------------------------------------------------
    // Make some decisions based on the current action

    clBool isImageString = clFalse;
    switch (C->action) {
        case CL_ACTION_NONE:
        case CL_ACTION_ERROR:
            FAIL();

        case CL_ACTION_GENERATE:
            isImageString = clTrue;
            if (!C->inputFilename) {
                if (params.formatName && !strcmp(params.formatName, "icc")) {
                    // Allow this; they are generating an ICC profile.
                } else {
                    clContextLogError(C, "Generate requires an image string for outputs other than ICC profiles.");
                    FAIL();
                }
            }
            break;

        case CL_ACTION_CALC:
            isImageString = clTrue;
            break;

        case CL_ACTION_CONVERT:
        case CL_ACTION_IDENTIFY:
        case CL_ACTION_MODIFY:
            break;
    }

    // -----------------------------------------------------------------------
    // Log action, Start the overall timer

    clContextLog(C, "action", 0, "%s [%d max threads]:", clActionToString(C, C->action), C->jobs);
    timerStart(&overall);

    // -----------------------------------------------------------------------
    // Load/create the original image

    timerStart(&t);

    int origImageStringDepth = params.bpc ? params.bpc : 8;
    if (isImageString) {
        if (params.autoGrade) {
            clContextLogError(C, "Autograde (-a) is incompatible with image strings");
            FAIL();
        }

        // This is just used for defaults, the generated image actually uses the output profile
        if (C->inputFilename) {
            image = clImageParseString(C, C->inputFilename, origImageStringDepth, NULL);
        } else {
            image = clImageCreate(C, 1, 1, origImageStringDepth, NULL);
        }
    } else {
        const char * originalFormatName = clFormatDetect(C, C->inputFilename);
        if (!originalFormatName) {
            FAIL();
        }

        if (!strcmp(originalFormatName, "icc")) {
            if (params.autoGrade) {
                clContextLogError(C, "Autograde (-a) is incompatible with ICC profiles");
                FAIL();
            }

            clProfile * profile = clProfileRead(C, C->inputFilename);
            if (!profile) {
                FAIL();
            }

            // Just jam the loaded profile into a 1x1 image
            image = clImageCreate(C, 1, 1, origImageStringDepth, profile);
            clProfileDestroy(C, profile);
        } else {
            const char * outFormatName = NULL;
            image = clContextRead(C, C->inputFilename, C->iccOverrideIn, &outFormatName);
            if (image && outFormatName) {
                clContextLog(C, "read", 1, "Successfully loaded format: %s", outFormatName);
            }
        }
    }

    if (image == NULL) {
        FAIL();
    }

    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    // -----------------------------------------------------------------------
    // Load HALD, if any

    if (params.hald) {
        timerStart(&t);

        haldImage = clContextRead(C, params.hald, NULL, NULL);
        if (!haldImage) {
            clContextLogError(C, "Can't read Hald CLUT: %s", params.hald);
            FAIL();
        }
        if (haldImage->width != haldImage->height) {
            clContextLogError(C, "Hald CLUT isn't square [%dx%d]: %s", haldImage->width, haldImage->height, params.hald);
            FAIL();
        }

        // Calc haldDims
        int i;
        for (i = 0; i < 32; ++i) {
            if ((i * i * i) == haldImage->width) {
                haldDims = i * i;
                break;
            }
        }
        if (haldDims == 0) {
            clContextLogError(C, "Hald CLUT dimensions aren't cubic [%dx%d]: %s", haldImage->width, haldImage->height, params.hald);
            FAIL();
        }

        clContextLog(C, "hald", 0, "Loaded %dx%dx%d Hald CLUT: %s", haldDims, haldDims, haldDims, params.hald);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Parse source image and conversion params, make decisions about dst

    // Populate srcInfo
    srcInfo.width = image->width;
    srcInfo.height = image->height;
    srcInfo.depth = image->depth;
    clProfileQuery(C, image->profile, &srcInfo.primaries, &srcInfo.curve, &srcInfo.luminance);
    if ((srcInfo.curve.type == CL_PCT_COMPLEX) && (srcInfo.curve.gamma > 0.0f)) {
        clContextLog(C, "info", 0, "Estimated source gamma: %g", srcInfo.curve.gamma);
    }

    // Start off dstInfo with srcInfo's values
    memcpy(&dstInfo, &srcInfo, sizeof(dstInfo));

    // Forget starting gamma and luminance if we're autograding (conversion params can still force values)
    if (params.autoGrade) {
        dstInfo.curve.type = CL_PCT_GAMMA;
        dstInfo.curve.gamma = 0;
        dstInfo.luminance = CL_LUMINANCE_UNSPECIFIED;
    }

    // Load output profile override, if any
    if (params.iccOverrideOut) {
        if (params.autoGrade) {
            clContextLogError(C, "Can't autograde (-a) along with a specified profile from disk (--iccout), please choose one or the other.");
            FAIL();
        }

        dstProfile = clProfileRead(C, params.iccOverrideOut);
        if (!dstProfile) {
            clContextLogError(C, "Invalid destination profile override: %s", params.iccOverrideOut);
            FAIL();
        }

        clProfileQuery(C, dstProfile, &dstInfo.primaries, &dstInfo.curve, &dstInfo.luminance);
        if ((dstInfo.curve.type == CL_PCT_COMPLEX) && (dstInfo.curve.gamma > 0.0f)) {
            clContextLog(C, "info", 0, "Estimated dst gamma: %g", dstInfo.curve.gamma);
        }

        clContextLog(C, "profile", 1, "Overriding dst profile with file: %s", params.iccOverrideOut);
    } else {
        // No output profile, allow profile overrides

        // Override primaries
        if (params.primaries[0] > 0.0f) {
            dstInfo.primaries.red[0] = params.primaries[0];
            dstInfo.primaries.red[1] = params.primaries[1];
            dstInfo.primaries.green[0] = params.primaries[2];
            dstInfo.primaries.green[1] = params.primaries[3];
            dstInfo.primaries.blue[0] = params.primaries[4];
            dstInfo.primaries.blue[1] = params.primaries[5];
            dstInfo.primaries.white[0] = params.primaries[6];
            dstInfo.primaries.white[1] = params.primaries[7];
        }

        // Override luminance
        if (params.luminance >= 0) {
            dstInfo.luminance = params.luminance;
        }

        // Override gamma
        if (params.gamma > 0.0f) {
            dstInfo.curve.type = params.curveType;
            dstInfo.curve.gamma = params.gamma;
        }
    }

    // Override width and height
    if ((params.resizeW > 0) || (params.resizeH > 0)) {
        if (params.resizeW <= 0) {
            dstInfo.width = (int)(((float)srcInfo.width / (float)srcInfo.height) * params.resizeH);
            dstInfo.height = params.resizeH;
        } else if (params.resizeH <= 0) {
            dstInfo.width = params.resizeW;
            dstInfo.height = (int)(((float)srcInfo.height / (float)srcInfo.width) * params.resizeW);
        } else {
            dstInfo.width = params.resizeW;
            dstInfo.height = params.resizeH;
        }
        if (dstInfo.width <= 0)
            dstInfo.width = 1;
        if (dstInfo.height <= 0)
            dstInfo.height = 1;
    }

    // Override depth
    {
        if (params.bpc > 0) {
            dstInfo.depth = params.bpc;
        }

        int bestDepth = CL_CLAMP(dstInfo.depth, 8, 16);
        if (params.formatName && (strcmp(params.formatName, "icc") != 0)) {
            bestDepth = clFormatBestDepth(C, params.formatName, dstInfo.depth);
        }
        if (dstInfo.depth != bestDepth) {
            clContextLog(C, "validate", 0, "Overriding output depth %d-bit -> %d-bit (format limitations)", dstInfo.depth, bestDepth);
            dstInfo.depth = bestDepth;
        }
    }

    // Color grading
    if (params.autoGrade) {
        COLORIST_ASSERT(dstProfile == NULL);

        clContextLog(C, "grading", 0, "Color grading ...");
        timerStart(&t);
        dstInfo.curve.type = CL_PCT_GAMMA;
        clImageColorGrade(C, image, dstInfo.depth, &dstInfo.luminance, &dstInfo.curve.gamma, C->verbose);
        clContextLog(C, "grading", 0, "Using maxLum: %d, gamma: %g", dstInfo.luminance, dstInfo.curve.gamma);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Profile params validation & profile creation

    // Create the destination profile, or clone the source one
    if (dstProfile == NULL) {
        if ((memcmp(&srcInfo.primaries, &dstInfo.primaries, sizeof(srcInfo.primaries)) != 0) || // Custom primaries
            (memcmp(&srcInfo.curve, &dstInfo.curve, sizeof(srcInfo.curve)) != 0) ||             // Custom curve
            (srcInfo.luminance != dstInfo.luminance) ||                                         // Custom luminance
            (params.description) ||                                                             // Custom description
            (params.copyright)                                                                  // custom copyright
        ) {
            // Primaries
            if ((dstInfo.primaries.red[0] <= 0.0f) || (dstInfo.primaries.red[1] <= 0.0f) || (dstInfo.primaries.green[0] <= 0.0f) ||
                (dstInfo.primaries.green[1] <= 0.0f) || (dstInfo.primaries.blue[0] <= 0.0f) || (dstInfo.primaries.blue[1] <= 0.0f) ||
                (dstInfo.primaries.white[0] <= 0.0f) || (dstInfo.primaries.white[1] <= 0.0f)) {
                clContextLogError(C, "Can't create destination profile, destination primaries are invalid");
                FAIL();
            }

            // Curve
            if (dstInfo.curve.type == CL_PCT_COMPLEX) {
                // TODO: Support/pass-through any source curve
                clContextLogError(C, "Can't create destination profile, tone curve cannot be created as it isn't just a simple gamma curve. Try choosing a new curve (-g) or autograding (-a)");
                FAIL();
            }
            if (dstInfo.curve.gamma <= 0.0f) {
                // TODO: Support/pass-through any source curve
                clContextLogError(C, "Can't create destination profile, gamma(%g) is invalid", dstInfo.curve.gamma);
                FAIL();
            }

            if (dstInfo.luminance < 0) {
                clContextLogError(C, "Can't create destination profile, luminance(%d) is invalid", dstInfo.luminance);
                FAIL();
            }

            // Description
            char * dstDescription = NULL;
            if (params.description) {
                dstDescription = clContextStrdup(C, params.description);
            } else {
                dstDescription = clGenerateDescription(C, &dstInfo.primaries, &dstInfo.curve, dstInfo.luminance);
            }

            clContextLog(C, "profile", 0, "Creating new destination ICC profile: \"%s\"", dstDescription);
            dstProfile = clProfileCreate(C, &dstInfo.primaries, &dstInfo.curve, dstInfo.luminance, dstDescription);
            clFree(dstDescription);

            // Copyright
            if (params.copyright) {
                clContextLog(C, "profile", 1, "Setting copyright: \"%s\"", params.copyright);
                clProfileSetMLU(C, dstProfile, "cprt", "en", "US", params.copyright);
            }
        } else {
            // just clone the source one
            clContextLog(C, "profile", 0, "Using unmodified source ICC profile: \"%s\"", image->profile->description);
            dstProfile = clProfileClone(C, image->profile);
        }
    }
    COLORIST_ASSERT(dstProfile);

    if (C->params.stripTags) {
        char * tagsBuffer = clContextStrdup(C, C->params.stripTags);
        char * tagName;
        for (tagName = strtok(tagsBuffer, ","); tagName != NULL; tagName = strtok(NULL, ",")) {
            if (clProfileRemoveTag(C, dstProfile, tagName, NULL)) {
                clContextLog(C, "modify", 0, "Stripping tag: '%s'", tagName);
            } else {
                clContextLog(C, "modify", 0, "Tag '%s' already absent, skipping strip", tagName);
            }
        }
        clProfileReload(C, dstProfile);
        clFree(tagsBuffer);
    }

    // -----------------------------------------------------------------------
    // Convert / recreate image with destination profile

    if (clProfileMatches(C, image->profile, dstProfile) && (image->depth == dstInfo.depth)) {
        clImageDebugDump(C, image, 0, 0, 0, 0, 1);
    } else {
        clImage * convertedImage =
            clImageConvert(C, image, dstInfo.depth, dstProfile, params.autoGrade ? CL_TONEMAP_OFF : params.tonemap, &params.tonemapParams);
        if (!convertedImage) {
            FAIL();
        }
        clImageDestroy(C, image);
        image = convertedImage;
    }

    // -----------------------------------------------------------------------
    // Crop

    if (C->action != CL_ACTION_IDENTIFY) {
        int crop[4];
        memcpy(crop, C->params.rect, 4 * sizeof(int));
        if (clImageAdjustRect(C, image, &crop[0], &crop[1], &crop[2], &crop[3])) {
            timerStart(&t);
            clContextLog(C,
                         "crop",
                         0,
                         "Cropping source image from %dx%d to: +%d+%d %dx%d",
                         image->width,
                         image->height,
                         crop[0],
                         crop[1],
                         crop[2],
                         crop[3]);
            image = clImageCrop(C, image, crop[0], crop[1], crop[2], crop[3], clFalse);
            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }
    }

    // -----------------------------------------------------------------------
    // Resize

    if (((dstInfo.width != srcInfo.width) || (dstInfo.height != srcInfo.height))) {
        clContextLog(C,
                     "resize",
                     0,
                     "Resizing %dx%d -> [filter:%s] -> %dx%d",
                     srcInfo.width,
                     srcInfo.height,
                     clFilterToString(C, params.resizeFilter),
                     dstInfo.width,
                     dstInfo.height);
        timerStart(&t);

        clImage * resizedImage = clImageResize(C, image, dstInfo.width, dstInfo.height, params.resizeFilter);
        if (!resizedImage) {
            clContextLogError(C, "Failed to resize image");
            FAIL();
        }

        clImageDestroy(C, image);
        image = resizedImage;

        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Composite

    if (C->params.compositeFilename) {
        clContextLog(C,
                     "composite",
                     0,
                     "Composition enabled. Reading: %s (%d bytes)",
                     C->params.compositeFilename,
                     clFileSize(C->params.compositeFilename));
        timerStart(&t);
        clImage * compositeImage = clContextRead(C, params.compositeFilename, NULL, NULL);
        if (compositeImage == NULL) {
            clContextLogError(C, "Can't load composite image, bailing out");
            FAIL();
        }
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

        clContextLog(C,
                     "composite",
                     0,
                     "Blending composite on top (%.2g gamma, %s, offset %d,%d)...",
                     params.compositeParams.gamma,
                     params.compositeParams.premultiplied ? "premultiplied" : "not premultiplied",
                     params.compositeParams.offsetX,
                     params.compositeParams.offsetY);
        timerStart(&t);
        params.compositeParams.srcTonemap = params.tonemap;
        memcpy(&params.compositeParams.srcParams, &params.tonemapParams, sizeof(clTonemapParams));
        clImage * blendedImage = clImageBlend(C, image, compositeImage, &params.compositeParams);
        if (!blendedImage) {
            clContextLogError(C, "Image blend failed, bailing out");
            clImageDestroy(C, blendedImage);
            FAIL();
        }
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

        clImageDestroy(C, image);
        image = blendedImage;
    }

    // -----------------------------------------------------------------------
    // Hald

    if (haldImage) {
        clContextLog(C, "hald", 0, "Performing Hald CLUT postprocessing...");
        timerStart(&t);

        clImage * appliedImage = clImageApplyHALD(C, image, haldImage, haldDims);
        if (!appliedImage) {
            clContextLogError(C, "Failed to apply HALD");
            FAIL();
        }

        clImageDestroy(C, image);
        image = appliedImage;

        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Rotate

    if (params.rotate != 0) {
        clContextLog(C, "rotate", 0, "Rotating image clockwise %dx...", params.rotate);
        timerStart(&t);

        clImage * rotatedImage = clImageRotate(C, image, params.rotate);
        if (rotatedImage) {
            clImageDestroy(C, image);
            image = rotatedImage;
        }

        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Dump pixels

    if (C->action == CL_ACTION_IDENTIFY) {
        int rect[4];
        memcpy(rect, C->params.rect, sizeof(rect));
        // if ((rect[2] < 0) && (rect[3] < 0)) {
        //     // Defaults for identify
        //     rect[2] = 3;
        //     rect[3] = 3;
        // }
        if (jsonOutput) {
            clImageDebugDumpJSON(C, jsonOutput, image, rect[0], rect[1], rect[2], rect[3]);
        } else {
            clImageDebugDump(C, image, rect[0], rect[1], rect[2], rect[3], 1);
        }
    }

    // -----------------------------------------------------------------------
    // Write

    if (C->outputFilename) {
        timerStart(&t);
        if (!strcmp(params.formatName, "icc")) {
            // Just dump out the profile to disk and bail out

            clContextLog(C, "encode", 0, "Writing ICC: %s", C->outputFilename);
            clProfileDebugDump(C, image->profile, C->verbose, 0);

            if (!clProfileWrite(C, image->profile, C->outputFilename)) {
                FAIL();
            }
            goto cleanup;
        } else {
            clContextLogWrite(C, C->outputFilename, params.formatName, &params.writeParams);
            if (!clContextWrite(C, image, C->outputFilename, params.formatName, &params.writeParams)) {
                FAIL();
            }
        }
        clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

        if ((strcmp(params.formatName, "icc") != 0) && params.stats) {
            clContextLog(C, "stats", 0, "Calculating conversion stats...");
            timerStart(&t);

            clImage * convertedImage = clContextRead(C, C->outputFilename, NULL, NULL);
            if (convertedImage) {
                clImageSignals signals;
                if (clImageCalcSignals(C, image, convertedImage, &signals)) {
                    clContextLog(C, "stats", 1, "MSE  (Lin) : %g", signals.mseLinear);
                    clContextLog(C, "stats", 1, "PSNR (Lin) : %g", signals.psnrLinear);
                    clContextLog(C, "stats", 1, "MSE  (2.2g): %g", signals.mseG22);
                    clContextLog(C, "stats", 1, "PSNR (2.2g): %g", signals.psnrG22);
                }
                clImageDestroy(C, convertedImage);
            } else {
                clContextLogError(C, "Failed to reload converted image, skipping conversion stats");
            }

            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }
    }

cleanup:
    if (image) {
        clImageDestroy(C, image);
    }
    if (haldImage) {
        clImageDestroy(C, haldImage);
    }
    if (dstProfile) {
        clProfileDestroy(C, dstProfile);
    }

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "%s complete.", clActionToString(C, C->action));
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
}
