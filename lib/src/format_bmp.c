// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Adapted from public MSDN docs

typedef struct BITMAPFILEHEADER
{
    // uint16_t bfType; // Commented out because the alignment of this struct is tragic. I'll just deal with this manually.
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef int32_t FXPT2DOT30;
typedef struct CIEXYZ
{
    FXPT2DOT30 ciexyzX;
    FXPT2DOT30 ciexyzY;
    FXPT2DOT30 ciexyzZ;
} CIEXYZ;

typedef struct CIEXYZTRIPLE
{
    CIEXYZ ciexyzRed;
    CIEXYZ ciexyzGreen;
    CIEXYZ ciexyzBlue;
} CIEXYZTRIPLE;

typedef struct BITMAPV5HEADER
{
    uint32_t bV5Size;
    int32_t bV5Width;
    int32_t bV5Height;
    uint16_t bV5Planes;
    uint16_t bV5BitCount;
    uint32_t bV5Compression;
    uint32_t bV5SizeImage;
    int32_t bV5XPelsPerMeter;
    int32_t bV5YPelsPerMeter;
    uint32_t bV5ClrUsed;
    uint32_t bV5ClrImportant;
    uint32_t bV5RedMask;
    uint32_t bV5GreenMask;
    uint32_t bV5BlueMask;
    uint32_t bV5AlphaMask;
    uint32_t bV5CSType;
    CIEXYZTRIPLE bV5Endpoints;
    uint32_t bV5GammaRed;
    uint32_t bV5GammaGreen;
    uint32_t bV5GammaBlue;
    uint32_t bV5Intent;
    uint32_t bV5ProfileData;
    uint32_t bV5ProfileSize;
    uint32_t bV5Reserved;
} BITMAPV5HEADER;

#define BI_RGB        0
#define BI_BITFIELDS  3

#define LCS_sRGB                'sRGB'
#define LCS_WINDOWS_COLOR_SPACE 'Win ' // Windows default color space
#define PROFILE_EMBEDDED        'MBED'

#define LCS_GM_ABS_COLORIMETRIC 8

// ---------------------------------------------------------------------------

// figures out the depth of the channel mask, then returns max(channelDepth, currentDepth) and the right shift
static int maskDepth(uint32_t mask, int currentDepth, int * channelDepth, int * rightShift)
{
    int depth = 0;
    *channelDepth = 0;
    *rightShift = 0;

    if (!mask)
        return currentDepth;

    // Shift out all trailing 0s
    while (!(mask & 1)) {
        mask >>= 1;
        ++(*rightShift);
    }

    // Find the first unset bit
    while (mask & (1 << (depth))) {
        ++depth;
    }
    *channelDepth = depth;
    return (depth > currentDepth) ? depth : currentDepth;
}

struct clImage * clFormatReadBMP(struct clContext * C, const char * formatName, struct clRaw * fileContents)
{
    clImage * image = NULL;
    clProfile * profile = NULL;
    const uint16_t expectedMagic = 0x4D42; // 'BM'
    uint16_t magic = 0;
    BITMAPFILEHEADER fileHeader;
    BITMAPV5HEADER info;
    int rDepth, gDepth, bDepth, aDepth;
    int rShift, gShift, bShift, aShift;
    int depth = 8;
    int packedPixelBytes = 0;
    uint32_t * packedPixels;
    int pixelCount;

    if (fileContents->size < (sizeof(magic) + sizeof(fileHeader))) {
        clContextLogError(C, "Truncated BMP");
        goto readCleanup;
    }

    memcpy(&magic, fileContents->ptr, sizeof(magic));
    if (magic != expectedMagic) {
        clContextLogError(C, "Unexpected magic (BM)");
        goto readCleanup;
    }

    memcpy(&fileHeader, fileContents->ptr + 2, sizeof(fileHeader));
    if (fileHeader.bfSize != fileContents->size) {
        clContextLogError(C, "Invalid BMP total size in file header");
        goto readCleanup;
    }

    memset(&info, 0, sizeof(info));
    memcpy(&info, fileContents->ptr + sizeof(magic) + sizeof(fileHeader), 4); // read bV5Size
    if ((info.bV5Size >= fileContents->size) || (info.bV5Size > sizeof(info))) {
        clContextLogError(C, "Invalid BMP info header size");
        goto readCleanup;
    }
    memcpy(&info, fileContents->ptr + 2 + sizeof(fileHeader), info.bV5Size); // read the whole header
    // TODO: Make decisions based on the size? (autodetect V4 or previous)

    if (info.bV5BitCount != 32) {
        clContextLogError(C, "Colorist currently only supports 32bit BMPs [%d detected]", info.bV5BitCount);
        goto readCleanup;
    }

    if (info.bV5CSType == PROFILE_EMBEDDED) {
        if ((sizeof(magic) + sizeof(fileHeader) + info.bV5ProfileData + info.bV5ProfileSize) > fileContents->size) {
            clContextLogError(C, "Invalid BMP ICC profile offset/size");
            goto readCleanup;
        } else {
            uint8_t * rawProfileData = fileContents->ptr + sizeof(magic) + sizeof(fileHeader) + info.bV5ProfileData;
            profile = clProfileParse(C, rawProfileData, info.bV5ProfileSize, NULL);
            if (profile == NULL) {
                clContextLogError(C, "Invalid ICC embedded profile");
                goto readCleanup;
            }
        }
    }

    if (info.bV5Compression == BI_BITFIELDS) {
        depth = maskDepth(info.bV5RedMask, depth, &rDepth, &rShift);
        depth = maskDepth(info.bV5GreenMask, depth, &gDepth, &gShift);
        depth = maskDepth(info.bV5BlueMask, depth, &bDepth, &bShift);
        depth = maskDepth(info.bV5AlphaMask, depth, &aDepth, &aShift);
    } else {
        if (info.bV5Compression != BI_RGB) {
            clContextLogError(C, "Unsupported BMP compression");
            goto readCleanup;
        }

        // Assume these masks/depths for BI_RGB
        info.bV5BlueMask = 255 << 0;
        info.bV5GreenMask = 255 << 8;
        info.bV5RedMask = 255 << 16;
        info.bV5AlphaMask = 255 << 24;
        rDepth = gDepth = bDepth = aDepth = 8;
    }

    if ((depth != 8) && (depth != 10)) {
        clContextLogError(C, "Invalid BMP depth [%d]", depth);
        goto readCleanup;
    }

    pixelCount = info.bV5Width * info.bV5Height;
    packedPixelBytes = sizeof(uint32_t) * pixelCount;
    packedPixels = clAllocate(packedPixelBytes);
    if ((fileHeader.bfOffBits + packedPixelBytes) > fileContents->size) {
        clContextLogError(C, "Truncated BMP (not enough pixel data)");
        goto readCleanup;
    }
    memcpy(packedPixels, fileContents->ptr + fileHeader.bfOffBits, packedPixelBytes);

    clImageLogCreate(C, info.bV5Width, info.bV5Height, depth, profile);
    image = clImageCreate(C, info.bV5Width, info.bV5Height, depth, profile);

    if (image->depth == 8) {
        int i;
        for (i = 0; i < pixelCount; ++i) {
            uint8_t * dstPixel = &image->pixels[i * 4];
            dstPixel[0] = (packedPixels[i] & info.bV5RedMask) >> rShift;
            dstPixel[1] = (packedPixels[i] & info.bV5GreenMask) >> gShift;
            dstPixel[2] = (packedPixels[i] & info.bV5BlueMask) >> bShift;
            if (aDepth > 0)
                dstPixel[3] = (packedPixels[i] & info.bV5AlphaMask) >> aShift;
            else
                dstPixel[3] = (1 << image->depth) - 1;
        }
    } else {
        // 10 bit
        int i;
        for (i = 0; i < pixelCount; ++i) {
            uint16_t * pixels = (uint16_t *)image->pixels;
            uint16_t * dstPixel = &pixels[i * 4];
            dstPixel[0] = (packedPixels[i] & info.bV5RedMask) >> rShift;
            dstPixel[1] = (packedPixels[i] & info.bV5GreenMask) >> gShift;
            dstPixel[2] = (packedPixels[i] & info.bV5BlueMask) >> bShift;
            if (aDepth > 0)
                dstPixel[3] = (packedPixels[i] & info.bV5AlphaMask) >> aShift;
            else
                dstPixel[3] = (1 << image->depth) - 1;
        }
    }

readCleanup:
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;

    COLORIST_UNUSED(formatName);
}

clBool clFormatWriteBMP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    clBool writeResult = clTrue;
    clRaw rawProfile;
    uint16_t magic = 0x4D42; // 'BM'
    BITMAPFILEHEADER fileHeader;
    BITMAPV5HEADER info;
    int packedPixelBytes = 0;
    uint32_t * packedPixels = NULL;
    int pixelCount = image->width * image->height;
    uint8_t *p;

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        goto writeCleanup;
    }

    if ((image->depth != 8) && (image->depth != 10)) {
        clContextLogError(C, "BMP writer can currently only handle 8 and 10 bit depths");
        goto writeCleanup;
    }

    memset(&info, 0, sizeof(info));
    info.bV5Size = sizeof(info);
    info.bV5Width = image->width;
    info.bV5Height = image->height;
    info.bV5Planes = 1;
    info.bV5BitCount = 32;
    info.bV5Compression = BI_BITFIELDS;
    info.bV5SizeImage = 0;
    info.bV5CSType = PROFILE_EMBEDDED;
    info.bV5Intent = LCS_GM_ABS_COLORIMETRIC;
    info.bV5ProfileData = sizeof(info);
    info.bV5ProfileSize = rawProfile.size;

    packedPixelBytes = sizeof(uint32_t) * info.bV5Width * info.bV5Height;
    packedPixels = clAllocate(packedPixelBytes);
    if (image->depth == 8) {
        int i;
        for (i = 0; i < pixelCount; ++i) {
            uint8_t * srcPixel = &image->pixels[i * 4];
            packedPixels[i] =
                (srcPixel[2] << 0) +  // B
                (srcPixel[1] << 8) +  // G
                (srcPixel[0] << 16) + // R
                (srcPixel[3] << 24);  // A
        }
        info.bV5BlueMask = 255 << 0;
        info.bV5GreenMask = 255 << 8;
        info.bV5RedMask = 255 << 16;
        info.bV5AlphaMask = 255 << 24;
    } else {
        // 10 bit
        int i;
        for (i = 0; i < pixelCount; ++i) {
            uint16_t * pixels = (uint16_t *)image->pixels;
            uint16_t * srcPixel = &pixels[i * 4];
            packedPixels[i] =
                ((srcPixel[2] & 1023) << 0) +  // B
                ((srcPixel[1] & 1023) << 10) + // G
                ((srcPixel[0] & 1023) << 20);  // R
            // (((srcPixel[3] >> 8) & 3) << 30); // no Alpha in 10 bit
        }
        info.bV5BlueMask = 1023 << 0;
        info.bV5GreenMask = 1023 << 10;
        info.bV5RedMask = 1023 << 20;
        info.bV5AlphaMask = 0; // no alpha in 10-bit BMPs, it behaves poorly with imagemagick
    }

    memset(&fileHeader, 0, sizeof(fileHeader));
    fileHeader.bfOffBits = sizeof(magic) + sizeof(fileHeader) + sizeof(info) + rawProfile.size;
    fileHeader.bfSize = fileHeader.bfOffBits + packedPixelBytes;

    clRawRealloc(C, output, fileHeader.bfSize);
#define APPEND(PTR, SIZE) memcpy(p, PTR, SIZE); p += SIZE;
    p = output->ptr;
    APPEND(&magic, sizeof(magic));
    APPEND(&fileHeader, sizeof(fileHeader));
    APPEND(&info, sizeof(info));
    APPEND(rawProfile.ptr, rawProfile.size);
    APPEND(packedPixels, packedPixelBytes);

writeCleanup:
    if (packedPixels) {
        clFree(packedPixels);
    }
    clRawFree(C, &rawProfile);
    return writeResult;

    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(writeParams);
}
