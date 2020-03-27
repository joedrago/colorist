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
clBool clFormatWriteTIFF(struct clContext * C,
                         struct clImage * image,
                         const char * formatName,
                         struct clRaw * output,
                         struct clWriteParams * writeParams);

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
    if (size <= 0) {
        return 0;
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
    if (size <= 0) {
        return 0;
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

static void errorHandler(thandle_t handle, const char * module, const char * fmt, va_list ap)
{
    (void)module;

    tiffCallbackInfo * ci = (tiffCallbackInfo *)handle;

    char tmp[256];
    vsnprintf(tmp, 255, fmt, ap);
    tmp[255] = 0;

    clContextLogError(ci->C, "TIFF Error: %s", tmp);
}

static void warningHandler(thandle_t handle, const char * module, const char * fmt, va_list ap)
{
    (void)module;

    tiffCallbackInfo * ci = (tiffCallbackInfo *)handle;

    char tmp[256];
    vsnprintf(tmp, 255, fmt, ap);
    tmp[255] = 0;

    clContextLogError(ci->C, "TIFF Warning: %s", tmp);
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
    uint16_t planarConfig = PLANARCONFIG_CONTIG;
    int orientation = ORIENTATION_TOPLEFT;
    int sampleFormat = SAMPLEFORMAT_UINT;
    uint8_t * iccBuf = NULL;
    int rowIndex, rowBytes;
    tiffCallbackInfo ci;
    uint8_t * pixels = NULL;
    clBool fp32 = clFalse;

    ci.C = C;
    ci.raw = input;
    ci.offset = 0;

    Timer t;
    timerStart(&t);

    TIFFSetErrorHandler(NULL);
    TIFFSetErrorHandlerExt(errorHandler);
    TIFFSetWarningHandler(NULL);
    TIFFSetWarningHandlerExt(warningHandler);

    tiff = TIFFClientOpen("tiff",
                          "rb",
                          (thandle_t)&ci,
                          (TIFFReadWriteProc)readCallback,
                          (TIFFReadWriteProc)writeCallback,
                          (TIFFSeekProc)seekCallback,
                          (TIFFCloseProc)closeCalllback,
                          (TIFFSizeProc)sizeCallback,
                          (TIFFMapFileProc)mapCallback,
                          (TIFFUnmapFileProc)unmapCallback);
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
    if ((channelCount != 1) && (channelCount != 3) && (channelCount != 4)) {
        clContextLogError(C, "unsupported channelCount(%d) from TIFF", channelCount);
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planarConfig);
    if ((planarConfig != PLANARCONFIG_CONTIG) && (planarConfig != PLANARCONFIG_SEPARATE)) {
        clContextLogError(C, "unsupported planarConfig(%u) from TIFF", planarConfig);
        goto readCleanup;
    }

    TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
    if (sampleFormat < 0) {
        sampleFormat = SAMPLEFORMAT_UINT;
    }
    TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &depth);
    if (depth <= 0) {
        // TODO: convert to 16bit
        clContextLogError(C, "cannot read depth from TIFF: '%s'");
        goto readCleanup;
    }
    if ((sampleFormat == SAMPLEFORMAT_IEEEFP) && (depth == 32)) {
        fp32 = clTrue;
    } else {
        if (sampleFormat != SAMPLEFORMAT_UINT) {
            clContextLogError(C, "unsupported sample format (%d) with depth(%d) from TIFF", sampleFormat, depth);
            goto readCleanup;
        }
        if ((depth != 1) && (depth != 8) && (depth != 16)) {
            clContextLogError(C, "unsupported uint depth(%d) from TIFF", depth);
            goto readCleanup;
        }
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

    uint8_t monochrome[2] = { 255, 0 };
    if (depth == 1) {
        uint16_t photometric = PHOTOMETRIC_MINISWHITE;
        TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photometric);
        if (photometric == PHOTOMETRIC_MINISBLACK) {
            monochrome[0] = 0;
            monochrome[1] = 255;
        }
    }

    clImageLogCreate(C, width, height, depth, profile);
    image = clImageCreate(C, width, height, depth, profile);

    if (fp32) {
        clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_F32);
        pixels = (uint8_t *)image->pixelsF32;
        rowBytes = image->width * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_F32);
    } else if ((depth == 1) || (depth == 8)) {
        clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U8);
        pixels = image->pixelsU8;
        rowBytes = image->width * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_U8);
    } else {
        clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U16);
        pixels = (uint8_t *)image->pixelsU16;
        rowBytes = image->width * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_U16);
    }

    if (planarConfig == PLANARCONFIG_CONTIG) {
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

            if (channelCount == 1) {
                // Expand grey in-place into RGBA, then fill A
                if (fp32) {
                    for (int x = image->width - 1; x >= 0; --x) {
                        float * srcPixel = (float *)&pixelRow[x * sizeof(float)];
                        float * dstPixel = (float *)&pixelRow[x * 4 * sizeof(float)];
                        dstPixel[3] = 1.0f;
                        dstPixel[2] = srcPixel[0];
                        dstPixel[1] = srcPixel[0];
                        dstPixel[0] = srcPixel[0];
                    }
                } else if (depth == 1) {
                    int shift = 7 - (image->width % 8);
                    for (int x = image->width - 1; x >= 0; --x) {
                        uint8_t mask = (uint8_t)(1 << (7 - shift));
                        uint8_t * srcPixel = &pixelRow[(x / 8) * sizeof(uint8_t)];
                        uint8_t * dstPixel = &pixelRow[x * 4 * sizeof(uint8_t)];
                        dstPixel[3] = 255;
                        dstPixel[2] = (srcPixel[0] & mask) ? monochrome[1] : monochrome[0];
                        dstPixel[1] = (srcPixel[0] & mask) ? monochrome[1] : monochrome[0];
                        dstPixel[0] = (srcPixel[0] & mask) ? monochrome[1] : monochrome[0];
                        --shift;
                        if (shift < 0) {
                            shift = 7;
                        }
                    }
                } else if (depth == 8) {
                    for (int x = image->width - 1; x >= 0; --x) {
                        uint8_t * srcPixel = &pixelRow[x * sizeof(uint8_t)];
                        uint8_t * dstPixel = &pixelRow[x * 4 * sizeof(uint8_t)];
                        dstPixel[3] = 255;
                        dstPixel[2] = srcPixel[0];
                        dstPixel[1] = srcPixel[0];
                        dstPixel[0] = srcPixel[0];
                    }
                } else {
                    for (int x = image->width - 1; x >= 0; --x) {
                        uint16_t * srcPixel = (uint16_t *)&pixelRow[x * sizeof(uint16_t)];
                        uint16_t * dstPixel = (uint16_t *)&pixelRow[x * 4 * sizeof(uint16_t)];
                        dstPixel[3] = 65535;
                        dstPixel[2] = srcPixel[0];
                        dstPixel[1] = srcPixel[0];
                        dstPixel[0] = srcPixel[0];
                    }
                }
            } else if (channelCount == 3) {
                // Expand RGB in-place into RGBA, then fill A
                if (fp32) {
                    for (int x = image->width - 1; x >= 0; --x) {
                        float * srcPixel = (float *)&pixelRow[x * 3 * sizeof(float)];
                        float * dstPixel = (float *)&pixelRow[x * 4 * sizeof(float)];
                        dstPixel[3] = 1.0f;
                        dstPixel[2] = srcPixel[2];
                        dstPixel[1] = srcPixel[1];
                        dstPixel[0] = srcPixel[0];
                    }
                } else if (depth == 1) {
                    int shift = 7 - (image->width % 8);
                    for (int x = image->width - 1; x >= 0; --x) {
                        uint8_t mask = (uint8_t)(1 << (7 - shift));
                        uint8_t * srcPixel = &pixelRow[((x * 3) / 8) * sizeof(uint8_t)];
                        uint8_t * dstPixel = &pixelRow[x * 4 * sizeof(uint8_t)];
                        dstPixel[3] = 255;
                        dstPixel[2] = (srcPixel[2] & mask) ? monochrome[1] : monochrome[0];
                        dstPixel[1] = (srcPixel[1] & mask) ? monochrome[1] : monochrome[0];
                        dstPixel[0] = (srcPixel[0] & mask) ? monochrome[1] : monochrome[0];
                        --shift;
                        if (shift < 0) {
                            shift = 7;
                        }
                    }
                } else if (depth == 8) {
                    for (int x = image->width - 1; x >= 0; --x) {
                        uint8_t * srcPixel = &pixelRow[x * 3 * sizeof(uint8_t)];
                        uint8_t * dstPixel = &pixelRow[x * 4 * sizeof(uint8_t)];
                        dstPixel[3] = 255;
                        dstPixel[2] = srcPixel[2];
                        dstPixel[1] = srcPixel[1];
                        dstPixel[0] = srcPixel[0];
                    }
                } else {
                    for (int x = image->width - 1; x >= 0; --x) {
                        uint16_t * srcPixel = (uint16_t *)&pixelRow[x * 1 * sizeof(uint16_t)];
                        uint16_t * dstPixel = (uint16_t *)&pixelRow[x * 4 * sizeof(uint16_t)];
                        dstPixel[3] = 65535;
                        dstPixel[2] = srcPixel[2];
                        dstPixel[1] = srcPixel[1];
                        dstPixel[0] = srcPixel[0];
                    }
                }
            }
        }
    } else if (planarConfig == PLANARCONFIG_SEPARATE) {
        if (channelCount <= 1) {
            clContextLogError(C, "unsupported planarConfig(%u) and channelCount(%d) from TIFF", planarConfig, channelCount);
            goto readCleanup;
        }

        uint8_t * readPixelRow = (uint8_t *)clAllocate(rowBytes);

        for (int channel = 0; channel < channelCount; ++channel) {
            for (rowIndex = 0; rowIndex < image->height; ++rowIndex) {
                uint8_t * pixelRow;
                if (orientation == ORIENTATION_TOPLEFT) {
                    pixelRow = &pixels[rowIndex * rowBytes];
                } else {
                    // ORIENTATION_BOTLEFT
                    pixelRow = &pixels[(image->height - 1 - rowIndex) * rowBytes];
                }
                if (TIFFReadScanline(tiff, readPixelRow, rowIndex, (uint16_t)channel) < 0) {
                    clContextLogError(C, "Failed to read TIFF scanline row %d", rowIndex);
                    clImageDestroy(C, image);
                    image = NULL;
                    clFree(readPixelRow);
                    goto readCleanup;
                }

                if (fp32) {
                    float * srcPixel = (float *)readPixelRow;
                    float * dstPixel = (float *)pixelRow;
                    for (int x = 0; x < image->width; ++x) {
                        dstPixel[channel] = srcPixel[0];
                        dstPixel += 4;
                        srcPixel += 1;
                    }
                } else if (depth == 1) {
                    int shift = 7;
                    uint8_t * srcPixel = (uint8_t *)readPixelRow;
                    uint8_t * dstPixel = (uint8_t *)pixelRow;
                    for (int x = 0; x < image->width; ++x) {
                        uint8_t mask = (uint8_t)(1 << (7 - shift));
                        dstPixel[channel] = (uint8_t)((srcPixel[0] & mask) << shift);
                        dstPixel += 4;
                        --shift;
                        if (shift < 0) {
                            shift = 7;
                            srcPixel += 1;
                        }
                    }
                } else if (depth == 8) {
                    uint8_t * srcPixel = (uint8_t *)readPixelRow;
                    uint8_t * dstPixel = (uint8_t *)pixelRow;
                    for (int x = 0; x < image->width; ++x) {
                        dstPixel[channel] = srcPixel[0];
                    }
                } else {
                    uint16_t * srcPixel = (uint16_t *)readPixelRow;
                    uint16_t * dstPixel = (uint16_t *)pixelRow;
                    for (int x = 0; x < image->width; ++x) {
                        dstPixel[channel] = srcPixel[0];
                        dstPixel += 4;
                        srcPixel += 1;
                    }
                }
            }
        }

        clFree(readPixelRow);

        // Fill A
        if (channelCount == 3) {
            for (rowIndex = 0; rowIndex < image->height; ++rowIndex) {
                uint8_t * pixelRow = &pixels[rowIndex * rowBytes];

                if (fp32) {
                    float * dstPixel = (float *)pixelRow;
                    for (int x = 0; x < image->width; ++x) {
                        dstPixel[3] = 1.0f;
                        dstPixel += 4;
                    }
                } else if ((depth == 1) || (depth == 8)) {
                    uint8_t * dstPixel = pixelRow;
                    for (int x = 0; x < image->width; ++x) {
                        dstPixel[3] = 255;
                        dstPixel += 4;
                    }
                } else {
                    uint16_t * dstPixel = (uint16_t *)pixelRow;
                    for (int x = 0; x < image->width; ++x) {
                        dstPixel[3] = 65535;
                        dstPixel += 4;
                    }
                }
            }
        }
    }

    C->readExtraInfo.decodeCodecSeconds = timerElapsedSeconds(&t);

readCleanup:
    if (tiff) {
        TIFFClose(tiff);
    }
    if (profile) {
        clProfileDestroy(C, profile);
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

    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        goto writeCleanup;
    }

    ci.C = C;
    ci.raw = output;
    ci.offset = 0;

    TIFFSetErrorHandler(NULL);
    TIFFSetErrorHandlerExt(errorHandler);
    TIFFSetWarningHandler(NULL);
    TIFFSetWarningHandlerExt(warningHandler);

    tiff = TIFFClientOpen("tiff",
                          "wb",
                          (thandle_t)&ci,
                          (TIFFReadWriteProc)readCallback,
                          (TIFFReadWriteProc)writeCallback,
                          (TIFFSeekProc)seekCallback,
                          (TIFFCloseProc)closeCalllback,
                          (TIFFSizeProc)sizeCallback,
                          (TIFFMapFileProc)mapCallback,
                          (TIFFUnmapFileProc)unmapCallback);
    if (!tiff) {
        clContextLogError(C, "cannot open TIFF for write");
        writeResult = clFalse;
        goto writeCleanup;
    }

    clBool fp32 = (image->depth == 32) ? clTrue : clFalse;
    if (fp32) {
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 32);
        TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);

        clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_F32);
        pixels = (uint8_t *)image->pixelsF32;
        rowBytes = image->width * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_F32);
    } else {
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, image->depth);
        TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        if (image->depth == 8) {
            clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_U8);
            pixels = image->pixelsU8;
            rowBytes = image->width * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_U8);
        } else {
            clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_U16);
            pixels = (uint8_t *)image->pixelsU16;
            rowBytes = image->width * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_U16);
        }
    }

    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, image->width);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, image->height);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);
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
