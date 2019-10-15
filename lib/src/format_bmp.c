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

#define BI_RGB 0
#define BI_BITFIELDS 3

#define LCS_sRGB 0x73524742 // 'sRGB'
// #define LCS_WINDOWS_COLOR_SPACE 0x57696e20 // 'Win ', Windows default color space
#define PROFILE_EMBEDDED 0x4d424544 // 'MBED'

#define LCS_GM_ABS_COLORIMETRIC 8

#define APPEND(PTR, SIZE) \
    memcpy(p, PTR, SIZE); \
    p += (SIZE);

struct clImage * clFormatReadBMP(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteBMP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

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

struct clImage * clFormatReadBMP(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);

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

    Timer t;
    timerStart(&t);

    if (input->size < (sizeof(magic) + sizeof(fileHeader))) {
        clContextLogError(C, "Truncated BMP");
        goto readCleanup;
    }

    memcpy(&magic, input->ptr, sizeof(magic));
    if (magic != expectedMagic) {
        clContextLogError(C, "Unexpected magic (BM)");
        goto readCleanup;
    }

    memcpy(&fileHeader, input->ptr + 2, sizeof(fileHeader));
    if (fileHeader.bfSize != input->size) {
        clContextLogError(C, "Invalid BMP total size in file header");
        goto readCleanup;
    }

    memset(&info, 0, sizeof(info));
    memcpy(&info, input->ptr + sizeof(magic) + sizeof(fileHeader), 4); // read bV5Size
    if ((info.bV5Size >= input->size) || (info.bV5Size > sizeof(info))) {
        clContextLogError(C, "Invalid BMP info header size");
        goto readCleanup;
    }
    memcpy(&info, input->ptr + 2 + sizeof(fileHeader), info.bV5Size); // read the whole header
    // TODO: Make decisions based on the size? (autodetect V4 or previous)

    if (info.bV5BitCount != 32) {
        clContextLogError(C, "Colorist currently only supports 32bit BMPs [%d detected]", info.bV5BitCount);
        goto readCleanup;
    }

    if (overrideProfile) {
        profile = clProfileClone(C, overrideProfile);
    } else if (info.bV5CSType == PROFILE_EMBEDDED) {
        if ((sizeof(magic) + sizeof(fileHeader) + info.bV5ProfileData + info.bV5ProfileSize) > input->size) {
            clContextLogError(C, "Invalid BMP ICC profile offset/size");
            goto readCleanup;
        } else {
            uint8_t * rawProfileData = input->ptr + sizeof(magic) + sizeof(fileHeader) + info.bV5ProfileData;
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
        info.bV5BlueMask = 255U << 0;
        info.bV5GreenMask = 255U << 8;
        info.bV5RedMask = 255U << 16;
        info.bV5AlphaMask = 255U << 24;
        rDepth = gDepth = bDepth = aDepth = 8;
        rShift = 0;
        gShift = 8;
        bShift = 16;
        aShift = 24;
    }

    if ((depth != 8) && (depth != 10)) {
        clContextLogError(C, "Invalid BMP depth [%d]", depth);
        goto readCleanup;
    }

    if (info.bV5Height < 0) {
        info.bV5Height *= -1;
    } else {
        clContextLogError(C, "Colorist currently only supports top-down BMPs, image will appear upside down!");
    }

    pixelCount = info.bV5Width * info.bV5Height;
    packedPixelBytes = sizeof(uint32_t) * pixelCount;
    packedPixels = clAllocate(packedPixelBytes);
    if ((fileHeader.bfOffBits + packedPixelBytes) > input->size) {
        clContextLogError(C, "Truncated BMP (not enough pixel data)");
        goto readCleanup;
    }
    memcpy(packedPixels, input->ptr + fileHeader.bfOffBits, packedPixelBytes);

    C->readExtraInfo.decodeCodecSeconds = timerElapsedSeconds(&t);

    clImageLogCreate(C, info.bV5Width, info.bV5Height, depth, profile);
    image = clImageCreate(C, info.bV5Width, info.bV5Height, depth, profile);
    clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U16);

    timerStart(&t);

    for (int i = 0; i < pixelCount; ++i) {
        uint16_t * dstPixel = &image->pixelsU16[i * 4];
        dstPixel[0] = (uint16_t)((packedPixels[i] & info.bV5RedMask) >> rShift);
        dstPixel[1] = (uint16_t)((packedPixels[i] & info.bV5GreenMask) >> gShift);
        dstPixel[2] = (uint16_t)((packedPixels[i] & info.bV5BlueMask) >> bShift);
        if (aDepth > 0)
            dstPixel[3] = (uint16_t)((packedPixels[i] & info.bV5AlphaMask) >> aShift);
        else
            dstPixel[3] = (uint16_t)((1 << depth) - 1);
    }

    C->readExtraInfo.decodeFillSeconds = timerElapsedSeconds(&t);

readCleanup:
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;
}

clBool clFormatWriteBMP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(writeParams);

    clBool writeResult = clTrue;
    uint16_t magic = 0x4D42; // 'BM'
    BITMAPFILEHEADER fileHeader;
    BITMAPV5HEADER info;
    int packedPixelBytes = 0;
    uint32_t * packedPixels = NULL;
    int pixelCount = image->width * image->height;
    uint8_t * p;

    clRaw rawProfile = CL_RAW_EMPTY;

    if ((image->depth != 8) && (image->depth != 10)) {
        clContextLogError(C, "BMP writer can currently only handle 8 and 10 bit depths");
        goto writeCleanup;
    }

    memset(&info, 0, sizeof(info));
    info.bV5Size = sizeof(info);
    info.bV5Width = image->width;
    info.bV5Height = -1 * image->height; // Negative indicates top-down
    info.bV5Planes = 1;
    info.bV5BitCount = 32;
    info.bV5Compression = BI_BITFIELDS;
    info.bV5SizeImage = 0;
    info.bV5Intent = LCS_GM_ABS_COLORIMETRIC;

    if (writeParams->writeProfile) {
        if (!clProfilePack(C, image->profile, &rawProfile)) {
            clContextLogError(C, "Failed to create ICC profile");
            goto writeCleanup;
        }
        info.bV5CSType = PROFILE_EMBEDDED;
        info.bV5ProfileData = sizeof(info);
        info.bV5ProfileSize = (uint32_t)rawProfile.size;
    } else {
        info.bV5CSType = LCS_sRGB;
    }

    clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_U16);

    packedPixelBytes = sizeof(uint32_t) * image->width * image->height;
    packedPixels = clAllocate(packedPixelBytes);
    if (image->depth == 8) {
        for (int i = 0; i < pixelCount; ++i) {
            uint16_t * srcPixel = &image->pixelsU16[i * CL_CHANNELS_PER_PIXEL];
            packedPixels[i] = (srcPixel[2] << 0) +  // B
                              (srcPixel[1] << 8) +  // G
                              (srcPixel[0] << 16) + // R
                              (srcPixel[3] << 24);  // A
        }
        info.bV5BlueMask = 255U << 0;
        info.bV5GreenMask = 255U << 8;
        info.bV5RedMask = 255U << 16;
        info.bV5AlphaMask = 255U << 24;
    } else {
        // 10 bit
        for (int i = 0; i < pixelCount; ++i) {
            uint16_t * srcPixel = &image->pixelsU16[i * CL_CHANNELS_PER_PIXEL];
            packedPixels[i] = ((srcPixel[2] & 1023) << 0) +  // B
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
    fileHeader.bfOffBits = (uint32_t)(sizeof(magic) + sizeof(fileHeader) + sizeof(info) + rawProfile.size);
    fileHeader.bfSize = fileHeader.bfOffBits + packedPixelBytes;

    clRawRealloc(C, output, fileHeader.bfSize);
    p = output->ptr;
    APPEND(&magic, sizeof(magic));
    APPEND(&fileHeader, sizeof(fileHeader));
    APPEND(&info, sizeof(info));
    if (rawProfile.size > 0) {
        APPEND(rawProfile.ptr, rawProfile.size);
    }
    APPEND(packedPixels, packedPixelBytes);

writeCleanup:
    if (packedPixels) {
        clFree(packedPixels);
    }
    clRawFree(C, &rawProfile);
    return writeResult;
}
