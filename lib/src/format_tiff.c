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

typedef struct tiffCallbackInfo
{
    struct clContext * C;
    clRaw * raw;
    toff_t offset;
} tiffCallbackInfo;

static tmsize_t readCallback(tiffCallbackInfo * ci, void * ptr, tmsize_t size)
{
    if ((ci->offset + size) > ci->raw->size) {
        size = (tmsize_t)(ci->raw->size - ci->offset);
    }
    memcpy(ptr, ci->raw->ptr + ci->offset, size);
    ci->offset += size;
    return size;
}

static tmsize_t writeCallback(tiffCallbackInfo * ci, void * ptr, tmsize_t size)
{
    if ((ci->offset + size) > ci->raw->size) {
        tmsize_t newSize = (tmsize_t)(ci->offset + size);
        clRawRealloc(ci->C, ci->raw, newSize);
    }
    memcpy(ci->raw->ptr + ci->offset, ptr, size);
    ci->offset += size;
    return size;
}

static toff_t seekCallback(tiffCallbackInfo * ci, toff_t off, int whence)
{
    switch (whence) {
        default:
        case SEEK_CUR:
            ci->offset += (uint32_t)off;
            break;
        case SEEK_SET:
            ci->offset = (uint32_t)off;
            break;
        case SEEK_END:
            ci->offset = ci->raw->size + (uint32_t)off;
            break;
    }
    return ci->offset;
}

static int closeCalllback(tiffCallbackInfo * ci)
{
    return 0;

    COLORIST_UNUSED(ci);
}

static toff_t sizeCallback(tiffCallbackInfo * ci)
{
    return ci->offset; // this seems bad
}

static int mapCallback(tiffCallbackInfo * ci, void ** base, toff_t * size)
{
    return 0;

    COLORIST_UNUSED(ci);
    COLORIST_UNUSED(base);
    COLORIST_UNUSED(size);
}

static void unmapCallback(tiffCallbackInfo * ci, void * base, toff_t size)
{
    COLORIST_UNUSED(ci);
    COLORIST_UNUSED(base);
    COLORIST_UNUSED(size);
}

struct clImage * clFormatReadTIFF(struct clContext * C, const char * formatName, struct clRaw * input)
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
    tiffCallbackInfo ci;

    ci.C = C;
    ci.raw = input;
    ci.offset = 0;

    tiff = TIFFClientOpen("tiff", "rb",
        (thandle_t)&ci,
        (TIFFReadWriteProc)readCallback, (TIFFReadWriteProc)writeCallback,
        (TIFFSeekProc)seekCallback, (TIFFCloseProc)closeCalllback,
        (TIFFSizeProc)sizeCallback,
        (TIFFMapFileProc)mapCallback, (TIFFUnmapFileProc)unmapCallback);
    if (!tiff) {
        clContextLogError(C, "cannot open TIFF for read");
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    if ((width <= 0) || (height <= 0)) {
        clContextLogError(C, "cannot read width and height from TIFF");
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &channelCount);
    if ((channelCount != 4)) {
        // TODO: support at least 3 channels
        clContextLogError(C, "unsupported channelCount(%d) from TIFF", channelCount);
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &depth);
    if ((depth <= 0)) {
        // TODO: convert to 16bit
        clContextLogError(C, "cannot read depth from TIFF: '%s'");
        goto readCleanup;
    }
    if ((depth != 8) && (depth != 16)) {
        clContextLogError(C, "unsupported depth(%d) from TIFF", depth);
        goto readCleanup;
    }

    if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &iccLen, &iccBuf)) {
        profile = clProfileParse(C, iccBuf, iccLen, NULL);
        if (!profile) {
            clContextLogError(C, "cannot parse ICC profile from TIFF");
            goto readCleanup;
        }
    }

    if (TIFFGetField(tiff, TIFFTAG_ORIENTATION, &orientation)) {
        if ((orientation != ORIENTATION_TOPLEFT) && (orientation != ORIENTATION_BOTLEFT)) {
            // TODO: Support other orientations
            clContextLogError(C, "Unsupported orientation (%d)", orientation);
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

    COLORIST_UNUSED(formatName);
}

clBool clFormatWriteTIFF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    clBool writeResult = clTrue;
    clRaw rawProfile;
    TIFF * tiff = NULL;
    int rowIndex, rowBytes;
    tiffCallbackInfo ci;

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        goto writeCleanup;
    }

    ci.C = C;
    ci.raw = output;
    ci.offset = 0;

    tiff = TIFFClientOpen("tiff", "wb",
        (thandle_t)&ci,
        (TIFFReadWriteProc)readCallback, (TIFFReadWriteProc)writeCallback,
        (TIFFSeekProc)seekCallback, (TIFFCloseProc)closeCalllback,
        (TIFFSizeProc)sizeCallback,
        (TIFFMapFileProc)mapCallback, (TIFFUnmapFileProc)unmapCallback);
    if (!tiff) {
        clContextLogError(C, "cannot open TIFF for write");
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

    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(writeParams);
}
