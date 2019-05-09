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

struct clImage * clFormatReadTIFF(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteTIFF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

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
            ci->offset += off;
            break;
        case SEEK_SET:
            ci->offset = off;
            break;
        case SEEK_END:
            ci->offset = ci->raw->size + off;
            break;
    }
    return ci->offset;
}

static int closeCalllback(tiffCallbackInfo * ci)
{
    COLORIST_UNUSED(ci);

    return 0;
}

static toff_t sizeCallback(tiffCallbackInfo * ci)
{
    return ci->offset; // this seems bad
}

static int mapCallback(tiffCallbackInfo * ci, void ** base, toff_t * size)
{
    COLORIST_UNUSED(ci);

    *base = NULL;
    *size = 0;

    return 0;
}

static void unmapCallback(tiffCallbackInfo * ci, void * base, toff_t size)
{
    COLORIST_UNUSED(ci);
    COLORIST_UNUSED(base);
    COLORIST_UNUSED(size);
}

struct clImage * clFormatReadTIFF(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);

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
    uint8_t * pixels = NULL;
    uint8_t * rgba8 = NULL;

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
    if ((channelCount != 3) && (channelCount != 4)) {
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

    if (overrideProfile) {
        profile = clProfileClone(C, overrideProfile);
    } else if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &iccLen, &iccBuf)) {
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
    if (depth == 8) {
        rgba8 = clAllocate(image->width * image->height * CL_CHANNELS_PER_PIXEL);
        pixels = rgba8;
        rowBytes = image->width * CL_CHANNELS_PER_PIXEL * 1;
    } else {
        pixels = (uint8_t *)image->pixels;
        rowBytes = image->width * CL_CHANNELS_PER_PIXEL * 2;
    }
    for (rowIndex = 0; rowIndex < image->height; ++rowIndex) {
        uint8_t * pixelRow;
        if (orientation == ORIENTATION_TOPLEFT) {
            pixelRow = &pixels[rowIndex * rowBytes];
        } else {
            // ORIENTATION_BOTLEFT
            pixelRow = &pixels[(image->height - 1 - rowIndex) * rowBytes];
        }
        if (TIFFReadScanline(tiff, pixelRow, rowIndex, 0) < 0) {
            clContextLogError(C, "Failed to read TIFF scanline row %d", rowIndex);
            clImageDestroy(C, image);
            image = NULL;
            goto readCleanup;
        }

        if (channelCount == 3) {
            // Expand RGB in-place into RGBA, then fill A
            if (depth == 8) {
                for (int x = image->width - 1; x >= 0; --x) {
                    uint8_t * srcPixel = &pixelRow[x * 3 * sizeof(uint8_t)];
                    uint8_t * dstPixel = &pixelRow[x * 4 * sizeof(uint8_t)];
                    dstPixel[0] = srcPixel[0];
                    dstPixel[1] = srcPixel[1];
                    dstPixel[2] = srcPixel[2];
                    dstPixel[3] = 255;
                }
            } else {
                for (int x = image->width - 1; x >= 0; --x) {
                    uint16_t * srcPixel = (uint16_t *)&pixelRow[x * 3 * sizeof(uint16_t)];
                    uint16_t * dstPixel = (uint16_t *)&pixelRow[x * 4 * sizeof(uint16_t)];
                    dstPixel[0] = srcPixel[0];
                    dstPixel[1] = srcPixel[1];
                    dstPixel[2] = srcPixel[2];
                    dstPixel[3] = 65535;
                }
            }
        }
    }

    if (rgba8) {
        clImageFromRGBA8(C, image, rgba8);
    }

readCleanup:
    if (tiff) {
        TIFFClose(tiff);
    }
    if (profile) {
        clProfileDestroy(C, profile);
    }
    if (rgba8) {
        clFree(rgba8);
    }
    return image;
}

clBool clFormatWriteTIFF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(writeParams);

    clBool writeResult = clTrue;
    TIFF * tiff = NULL;
    int rowIndex, rowBytes;
    tiffCallbackInfo ci;
    uint8_t * pixels = NULL;
    uint8_t * rgba8 = NULL;

    clRaw rawProfile = CL_RAW_EMPTY;
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

    if (image->depth == 8) {
        rgba8 = clAllocate(image->width * image->height * CL_CHANNELS_PER_PIXEL);
        clImageToRGBA8(C, image, rgba8);
        pixels = rgba8;
        rowBytes = image->width * CL_CHANNELS_PER_PIXEL * 1;
    } else {
        pixels = (uint8_t *)image->pixels;
        rowBytes = image->width * CL_CHANNELS_PER_PIXEL * 2;
    }

    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, image->width);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, image->height);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, image->depth);
    TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tiff, rowBytes));

    if (writeParams->writeProfile) {
        TIFFSetField(tiff, TIFFTAG_ICCPROFILE, rawProfile.size, rawProfile.ptr);
    }

    for (rowIndex = 0; rowIndex < image->height; ++rowIndex) {
        uint8_t * pixelRow = &pixels[rowIndex * rowBytes];
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
