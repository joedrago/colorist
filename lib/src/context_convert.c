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

struct ImageInfo
{
    int width;
    int height;
    int depth;

    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance;
};

int clContextConvert(clContext * C)
{
    Timer overall, t;
    int returnCode = 0;

    // Goals
    clImage * srcImage = NULL;
    clImage * dstImage = NULL;
    clProfile * dstProfile = NULL;

    // Information about the src&dst images, used to make all decisions
    struct ImageInfo srcInfo;
    struct ImageInfo dstInfo;

    // Hald CLUT
    clImage * haldImage = NULL;
    int haldDims = 0;

    clConversionParams params;
    memcpy(&params, &C->params, sizeof(params));

    if (!params.formatName)
        params.formatName = clFormatDetect(C, C->outputFilename);
    if (!params.formatName) {
        clContextLogError(C, "Unknown output file format: %s", C->outputFilename);
        FAIL();
    }

    clContextLog(C, "action", 0, "Convert: %s -> %s", C->inputFilename, C->outputFilename);
    timerStart(&overall);

    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    timerStart(&t);
    srcImage = clContextRead(C, C->inputFilename, C->iccOverrideIn, NULL);
    if (srcImage == NULL) {
        return 1;
    }
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    if (!strcmp(params.formatName, "icc")) {
        // Just dump out the profile to disk and bail out

        clContextLog(C, "encode", 0, "Writing ICC: %s", C->outputFilename);
        clProfileDebugDump(C, srcImage->profile, C->verbose, 0);

        if (!clProfileWrite(C, srcImage->profile, C->outputFilename)) {
            FAIL();
        }
        goto convertCleanup;
    }

    // Load HALD, if any
    if (params.hald) {
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
        {
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
        }
    }

    int crop[4];
    memcpy(crop, C->params.rect, 4 * sizeof(int));
    if (clImageAdjustRect(C, srcImage, &crop[0], &crop[1], &crop[2], &crop[3])) {
        timerStart(&t);
        clContextLog(C, "crop", 0, "Cropping source image from %dx%d to: +%d+%d %dx%d", srcImage->width, srcImage->height, crop[0], crop[1], crop[2], crop[3]);
        srcImage = clImageCrop(C, srcImage, crop[0], crop[1], crop[2], crop[3], clFalse);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

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
    if (params.autoGrade) {
        dstInfo.curve.type = CL_PCT_GAMMA;
        dstInfo.curve.gamma = 0;
        dstInfo.luminance = 0;
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
        dstInfo.luminance = (dstInfo.luminance != 0) ? dstInfo.luminance : COLORIST_DEFAULT_LUMINANCE;
        if ((dstInfo.curve.type != CL_PCT_GAMMA) && (dstInfo.curve.gamma > 0.0f)) {
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
        if (params.luminance > 0) {
            dstInfo.luminance = params.luminance;
        }

        // Override gamma
        if (params.gamma > 0.0f) {
            dstInfo.curve.type = CL_PCT_GAMMA;
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
        if (params.bpp > 0) {
            dstInfo.depth = params.bpp;
        }

        int bestDepth = clFormatBestDepth(C, params.formatName, dstInfo.depth);
        if (dstInfo.depth != bestDepth) {
            clContextLog(C, "validate", 0, "Overriding output depth %d-bit -> %d-bit (format limitations)", dstInfo.depth, bestDepth);
            dstInfo.depth = bestDepth;
        }
    }

    // -----------------------------------------------------------------------
    // Resize, if necessary

    if (((dstInfo.width != srcInfo.width) || (dstInfo.height != srcInfo.height))) {
        clContextLog(C, "resize", 0, "Resizing %dx%d -> [filter:%s] -> %dx%d", srcInfo.width, srcInfo.height, clFilterToString(C, params.resizeFilter), dstInfo.width, dstInfo.height);
        timerStart(&t);

        clImage * resizedImage = clImageResize(C, srcImage, dstInfo.width, dstInfo.height, params.resizeFilter);
        if (!resizedImage) {
            clContextLogError(C, "Failed to resize image");
            FAIL();
        }

        clImageDestroy(C, srcImage);
        srcImage = resizedImage;

        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    // -----------------------------------------------------------------------
    // Color grading

    if (params.autoGrade) {
        COLORIST_ASSERT(dstProfile == NULL);

        clContextLog(C, "grading", 0, "Color grading ...");
        timerStart(&t);
        dstInfo.curve.type = CL_PCT_GAMMA;
        clImageColorGrade(C, srcImage, params.jobs, dstInfo.depth, &dstInfo.luminance, &dstInfo.curve.gamma, C->verbose);
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
            (params.description) ||                                                      // Custom description
            (params.copyright)                                                           // custom copyright
            )
        {
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
            clContextLog(C, "profile", 0, "Using unmodified source ICC profile: \"%s\"", srcImage->profile->description);
            dstProfile = clProfileClone(C, srcImage->profile);
        }
    }

    dstImage = clImageConvert(C, srcImage, params.jobs, dstInfo.width, dstInfo.height, dstInfo.depth, dstProfile, params.autoGrade ? CL_TONEMAP_OFF : params.tonemap);
    if (!dstImage) {
        FAIL();
    }

    if (haldImage) {
        clContextLog(C, "hald", 0, "Performing Hald CLUT postprocessing...");
        timerStart(&t);

        clImage * appliedImage = clImageApplyHALD(C, dstImage, haldImage, haldDims);
        if (!appliedImage) {
            clContextLogError(C, "Failed to apply HALD");
            FAIL();
        }

        clImageDestroy(C, dstImage);
        dstImage = appliedImage;

        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    {
        clFormat * format = clContextFindFormat(C, params.formatName);
        COLORIST_ASSERT(format);
        if (format->usesRate && format->usesQuality) {
            clContextLog(C, "encode", 0, "Writing %s [%s:%d]: %s", format->description, (params.jp2rate) ? "R" : "Q", (params.jp2rate) ? params.jp2rate : params.quality, C->outputFilename);
        } else if (format->usesQuality) {
            clContextLog(C, "encode", 0, "Writing %s [Q:%d]: %s", format->description, params.quality, C->outputFilename);
        } else {
            clContextLog(C, "encode", 0, "Writing %s: %s", format->description, C->outputFilename);
        }
    }
    timerStart(&t);
    if (!clContextWrite(C, dstImage, C->outputFilename, params.formatName, params.quality, params.jp2rate)) {
        FAIL();
    }
    clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

convertCleanup:
    if (dstProfile)
        clProfileDestroy(C, dstProfile);
    if (srcImage)
        clImageDestroy(C, srcImage);
    if (dstImage)
        clImageDestroy(C, dstImage);
    if (haldImage)
        clImageDestroy(C, haldImage);

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "Conversion complete.");
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
}
