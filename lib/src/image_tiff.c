// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include "tiffio.h"

#include <string.h>

clImage * clImageReadTIFF(struct clContext * C, const char * filename)
{
    clProfile * profile = NULL;
    clImage * image = NULL;
    TIFF * tiff;
    int width = 0;
    int height = 0;
    int depth = 0;
    int iccLen = 0;
    int channelCount = 0;
    int orientation = ORIENTATION_TOPLEFT;
    uint8_t * iccBuf = NULL;
    int rowIndex, rowBytes;

    tiff = TIFFOpen(filename, "r");
    if (!tiff) {
        clContextLogError(C, "cannot open TIFF for read: '%s'", filename);
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    if ((width <= 0) || (height <= 0)) {
        clContextLogError(C, "cannot read width and height from TIFF: '%s'", filename);
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &channelCount);
    if ((channelCount != 4)) {
        // TODO: support at least 3 channels
        clContextLogError(C, "unsupported channelCount(%d) from TIFF: '%s'", channelCount, filename);
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &depth);
    if ((depth <= 0)) {
        // TODO: convert to 16bit
        clContextLogError(C, "cannot read depth from TIFF: '%s'", filename);
        goto readCleanup;
    }
    if ((depth != 8) && (depth != 16)) {
        clContextLogError(C, "unsupported depth(%d) from TIFF: '%s'", depth, filename);
        goto readCleanup;
    }

    if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &iccLen, &iccBuf)) {
        profile = clProfileParse(C, iccBuf, iccLen, NULL);
        if (!profile) {
            clContextLogError(C, "cannot parse ICC profile from TIFF: '%s'", filename);
            goto readCleanup;
        }
    }

    if (TIFFGetField(tiff, TIFFTAG_ORIENTATION, &orientation)) {
        if ((orientation != ORIENTATION_TOPLEFT) && (orientation != ORIENTATION_BOTLEFT)) {
            // TODO: Support other orientations
            clContextLogError(C, "Unsupported orientation (%d): '%s'", orientation, filename);
            goto readCleanup;
        }
    } else {
        // ?
        orientation = ORIENTATION_TOPLEFT;
    }

    clImageLogCreate(C, width, height, depth, profile);
    image = clImageCreate(C, width, height, depth, profile);
    rowBytes = image->width * 4 * clDepthToBytes(C, image->depth);
    for (rowIndex = 0; rowIndex < image->height; ++rowIndex) {
        uint8_t * pixelRow;
        if (orientation == ORIENTATION_TOPLEFT) {
            pixelRow = &image->pixels[rowIndex * rowBytes];
        } else {
            // ORIENTATION_BOTLEFT
            pixelRow = &image->pixels[(image->height - 1 - rowIndex) * rowBytes];
        }
        if (TIFFReadScanline(tiff, pixelRow, rowIndex, 0) < 0) {
            clContextLogError(C, "Failed to read TIFF scanline row %d", rowIndex);
            clImageDestroy(C, image);
            image = NULL;
            goto readCleanup;
        }
    }

readCleanup:
    if (tiff) {
        TIFFClose(tiff);
    }
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;
}

clBool clImageWriteTIFF(struct clContext * C, clImage * image, const char * filename)
{
    clBool writeResult = clTrue;
    clRaw rawProfile;
    TIFF * tiff;
    int rowIndex, rowBytes;

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        goto writeCleanup;
    }

    tiff = TIFFOpen(filename, "w");
    if (!tiff) {
        clContextLogError(C, "cannot open TIFF for write: '%s'", filename);
        writeResult = clFalse;
        goto writeCleanup;
    }

    rowBytes = image->width * 4 * clDepthToBytes(C, image->depth);
    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, image->width);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, image->height);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, image->depth);
    TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tiff, rowBytes));
    TIFFSetField(tiff, TIFFTAG_ICCPROFILE, rawProfile.size, rawProfile.ptr);

    for (rowIndex = 0; rowIndex < image->height; ++rowIndex) {
        uint8_t * pixelRow = &image->pixels[rowIndex * rowBytes];
        if (TIFFWriteScanline(tiff, pixelRow, rowIndex, 0) < 0) {
            clContextLogError(C, "Failed to write TIFF scanline row %d", rowIndex);
            writeResult = clFalse;
            goto writeCleanup;
        }
    }

writeCleanup:
    if (tiff) {
        TIFFClose(tiff);
    }
    clRawFree(C, &rawProfile);
    return writeResult;
}
