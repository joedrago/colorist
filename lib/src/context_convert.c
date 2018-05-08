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
    clImage * dstImage = NULL;

    int crop[4];

    clConversionParams params;
    memcpy(&params, &C->params, sizeof(params));

    if (params.format == CL_FORMAT_AUTO)
        params.format = clFormatDetect(C, C->outputFilename);
    if (params.format == CL_FORMAT_ERROR) {
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

    if ((params.format == CL_FORMAT_JP2) && (params.jp2rate > 0)) {
        int estimatedFileSizeKB = (8 + ((srcImage->width * srcImage->height * ((srcImage->depth == 16) ? 8 : 4)) / params.jp2rate)) / 1024;
        clContextLog(C, "estimate", 0, "JP2 [R:%d] estimated filesize: %d KB", params.jp2rate, estimatedFileSizeKB);
    }

    if (params.format == CL_FORMAT_ICC) {
        // Just dump out the profile to disk and bail out

        clContextLog(C, "encode", 0, "Writing ICC: %s", C->outputFilename);
        clProfileDebugDump(C, srcImage->profile, C->verbose, 0);

        if (!clProfileWrite(C, srcImage->profile, C->outputFilename)) {
            FAIL();
        }
        goto convertCleanup;
    }

    memcpy(crop, C->params.rect, 4 * sizeof(int));
    if (clImageAdjustRect(C, srcImage, &crop[0], &crop[1], &crop[2], &crop[3])) {
        timerStart(&t);
        clContextLog(C, "crop", 0, "Cropping source image from %dx%d to: +%d+%d %dx%d", srcImage->width, srcImage->height, crop[0], crop[1], crop[2], crop[3]);
        srcImage = clImageCrop(C, srcImage, crop[0], crop[1], crop[2], crop[3], clFalse);
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    dstImage = clImageConvert(C, srcImage, &params);
    if (!dstImage) {
        FAIL();
    }

    switch (params.format) {
        case CL_FORMAT_JP2:
            clContextLog(C, "encode", 0, "Writing JP2 [%s:%d]: %s", (params.jp2rate) ? "R" : "Q", (params.jp2rate) ? params.jp2rate : params.quality, C->outputFilename);
            break;
        case CL_FORMAT_JPG:
            clContextLog(C, "encode", 0, "Writing JPG [Q:%d]: %s", params.quality, C->outputFilename);
            break;
        case CL_FORMAT_WEBP:
            clContextLog(C, "encode", 0, "Writing WebP [Q:%d]: %s", params.quality, C->outputFilename);
            break;
        default:
            clContextLog(C, "encode", 0, "Writing: %s", C->outputFilename);
            break;
    }
    timerStart(&t);
    if (!clContextWrite(C, dstImage, C->outputFilename, params.format, params.quality, params.jp2rate)) {
        FAIL();
    }
    clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

convertCleanup:
    if (srcImage)
        clImageDestroy(C, srcImage);
    if (dstImage)
        clImageDestroy(C, dstImage);

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "Conversion complete.");
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
}
