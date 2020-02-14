// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/image.h"
#include "colorist/profile.h"

#include <string.h>

int clContextGenerate(clContext * C, struct cJSON * output)
{
    clProfile * dstProfile = NULL;
    const char * outputFileFormat = C->params.formatName;
    const char * action = "generate";

    if (C->outputFilename) {
        if (outputFileFormat == NULL)
            outputFileFormat = clFormatDetect(C, C->outputFilename);
        if (outputFileFormat == NULL) {
            return 1;
        }
        clContextLog(C, "action", 0, "Generating: %s", C->outputFilename);
    } else {
        clContextLog(C, "action", 0, "Calc: Generating an image and displaying its final pixels");
        action = "calc";
    }

    clWriteParams writeParams;
    memcpy(&writeParams, &C->params.writeParams, sizeof(writeParams));

    Timer overall;
    timerStart(&overall);

    if (outputFileFormat && !strcmp(outputFileFormat, "icc")) {
        if (C->inputFilename != NULL) {
            clContextLogError(C, "generate cannot accept an image string to generate a .icc file.");
            return 1;
        }
    } else {
        if (C->inputFilename == NULL) {
            clContextLogError(C, "generate and calc require an image string.");
            return 1;
        }
    }

    if (C->params.iccOverrideOut) {
        dstProfile = clProfileRead(C, C->params.iccOverrideOut);
        if (!dstProfile) {
            clContextLogError(C, "Invalid destination profile override: %s", C->params.iccOverrideOut);
            return 1;
        }
    } else {
        clProfilePrimaries primaries;
        if (C->params.primaries[0] <= 0.0f) {
            clBool ret = clContextGetStockPrimaries(C, "bt709", &primaries);
            COLORIST_ASSERT(ret == clTrue);
            (void)ret; // unused in Release
            clContextLog(C, action, 1, "No primaries specified (-p). Using default sRGB (BT.709) primaries.");
        } else {
            primaries.red[0] = C->params.primaries[0];
            primaries.red[1] = C->params.primaries[1];
            primaries.green[0] = C->params.primaries[2];
            primaries.green[1] = C->params.primaries[3];
            primaries.blue[0] = C->params.primaries[4];
            primaries.blue[1] = C->params.primaries[5];
            primaries.white[0] = C->params.primaries[6];
            primaries.white[1] = C->params.primaries[7];
        }

        clProfileCurve curve;
        curve.type = C->params.curveType;
        if (C->params.gamma <= 0.0f) {
            clContextLog(C, action, 1, "No gamma specified (-g). Using default sRGB gamma.");
            curve.gamma = COLORIST_SRGB_GAMMA;
        } else {
            curve.gamma = C->params.gamma;
        }

        int luminance = C->params.luminance;
        if (luminance <= 0) {
            luminance = CL_LUMINANCE_UNSPECIFIED;
        }

        char * description = NULL;
        if (C->params.description) {
            description = clContextStrdup(C, C->params.description);
        } else {
            description = clGenerateDescription(C, &primaries, &curve, luminance);
        }

        clContextLog(C, action, 0, "Generating ICC profile: \"%s\"", description);
        dstProfile = clProfileCreate(C, &primaries, &curve, luminance, description);
        clFree(description);

        if (C->params.copyright) {
            clContextLog(C, action, 1, "Setting copyright: \"%s\"", C->params.copyright);
            clProfileSetMLU(C, dstProfile, "cprt", "en", "US", C->params.copyright);
        }
    }

    if (C->inputFilename) {
        int depth = C->params.bpc;
        if (depth == 0) {
            clContextLog(C, action, 1, "No bits per pixel specified (-b). Setting to 8-bit.");
            depth = 8;
        }

        if (C->outputFilename && (depth != 8) && outputFileFormat && (strcmp(outputFileFormat, "icc") != 0)) {
            int bestDepth = clFormatBestDepth(C, outputFileFormat, depth);
            if (depth != bestDepth) {
                clContextLog(C, "validate", 0, "Overriding output depth %d-bit -> %d-bit (format limitations)", depth, bestDepth);
                depth = bestDepth;
            }
        }

        clImage * image = clImageParseString(C, C->inputFilename, depth, dstProfile);
        if (image == NULL) {
            clProfileDestroy(C, dstProfile);
            return 1;
        }

        if (C->params.rotate != 0) {
            clContextLog(C, "rotate", 0, "Rotating image clockwise %dx...", C->params.rotate);

            Timer t;
            timerStart(&t);

            clImage * rotatedImage = clImageRotate(C, image, C->params.rotate);
            if (rotatedImage) {
                clImageDestroy(C, image);
                image = rotatedImage;
            }

            clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
        }

        if (C->outputFilename) {
            clContextLogWrite(C, C->outputFilename, C->params.formatName, &writeParams);
            clImageDebugDump(C, image, C->params.rect[0], C->params.rect[1], C->params.rect[2], C->params.rect[3], 0);
            if (!clContextWrite(C, image, C->outputFilename, outputFileFormat, &writeParams)) {
                clImageDestroy(C, image);
                clProfileDestroy(C, dstProfile);
                return 1;
            }
        } else {
            int rect[4];
            memcpy(rect, C->params.rect, sizeof(int) * 4);
            if ((rect[0] == 0) && (rect[1] == 0) && (rect[2] == -1) && (rect[3] == -1)) {
                // rect is unset, use the whole image
                rect[2] = image->width;
                rect[3] = image->height;
            }
            if (output) {
                clImageDebugDumpJSON(C, output, image, rect[0], rect[1], rect[2], rect[3]);
            } else {
                clImageDebugDump(C, image, rect[0], rect[1], rect[2], rect[3], 0);
            }
        }
        clImageDestroy(C, image);
    } else {
        clContextLog(C, action, 0, "Writing ICC: %s", C->outputFilename);
        clProfileDebugDump(C, dstProfile, C->verbose, 0);
        if (!clProfileWrite(C, dstProfile, C->outputFilename)) {
            clProfileDestroy(C, dstProfile);
            return 1;
        }
    }

    clProfileDestroy(C, dstProfile);
    if (C->outputFilename) {
        clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
        clContextLog(C, "action", 0, "Generation complete (%g sec).", timerElapsedSeconds(&overall));
    } else {
        clContextLog(C, "action", 0, "Calc complete (%g sec).", timerElapsedSeconds(&overall));
    }
    return 0;
}
