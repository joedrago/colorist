// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include "png.h"

#include <string.h>

struct clImage * clFormatReadPNG(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWritePNG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct readInfo
{
    struct clContext * C;
    clRaw * src;
    png_size_t offset;
};

static void readCallback(png_structp png, png_bytep data, png_size_t length)
{
    struct readInfo * ri = (struct readInfo *)png_get_io_ptr(png);
    if ((ri->offset + length) <= ri->src->size) {
        memcpy(data, ri->src->ptr + ri->offset, length);
    }
    ri->offset += length;
}

struct clImage * clFormatReadPNG(struct clContext * C, const char * formatName, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);

    clImage * image = NULL;
    png_bytep * rowPointers = NULL;

    if (png_sig_cmp(input->ptr, 0, 8)) {
        clContextLogError(C, "not a PNG");
        return NULL;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    COLORIST_ASSERT(png && info);

    if (setjmp(png_jmpbuf(png))) {
        if (rowPointers) {
            clFree(rowPointers);
        }
        if (image) {
            clImageDestroy(C, image);
        }
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    struct readInfo ri;
    ri.C = C;
    ri.src = input;
    ri.offset = 0;

    png_set_read_fn(png, &ri, readCallback);
    png_read_info(png, info);

    clProfile * profile = NULL;

    char * iccpProfileName;
    int iccpCompression;
    unsigned char * iccpData;
    png_uint_32 iccpDataLen;

    if (png_get_iCCP(png, info, &iccpProfileName, &iccpCompression, &iccpData, &iccpDataLen) == PNG_INFO_iCCP) {
        profile = clProfileParse(C, iccpData, iccpDataLen, iccpProfileName);
    }

    int rawWidth = png_get_image_width(png, info);
    int rawHeight = png_get_image_height(png, info);
    png_byte rawColorType = png_get_color_type(png, info);
    png_byte rawBitDepth = png_get_bit_depth(png, info);

    if (rawColorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if ((rawColorType == PNG_COLOR_TYPE_GRAY) && (rawBitDepth < 8)) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    if ((rawColorType == PNG_COLOR_TYPE_RGB) ||
        (rawColorType == PNG_COLOR_TYPE_GRAY) ||
        (rawColorType == PNG_COLOR_TYPE_PALETTE))
    {
        png_set_filler(png, 0xFFFF, PNG_FILLER_AFTER);
    }

    if ((rawColorType == PNG_COLOR_TYPE_GRAY) ||
        (rawColorType == PNG_COLOR_TYPE_GRAY_ALPHA))
    {
        png_set_gray_to_rgb(png);
    }

    int imgBitDepth = 8;
    int imgBytesPerChannel = 1;
    if (rawBitDepth == 16) {
        png_set_swap(png);
        imgBitDepth = 16;
        imgBytesPerChannel = 2;
    }

    png_read_update_info(png, info);

    clImageLogCreate(C, rawWidth, rawHeight, imgBitDepth, profile);
    image = clImageCreate(C, rawWidth, rawHeight, imgBitDepth, profile);
    if (profile) {
        clProfileDestroy(C, profile);
    }
    rowPointers = (png_bytep *)clAllocate(sizeof(png_bytep) * rawHeight);
    if (imgBytesPerChannel == 1) {
        uint8_t * pixels = image->pixels;
        for (int y = 0; y < rawHeight; ++y) {
            rowPointers[y] = &pixels[4 * y * rawWidth];
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (int y = 0; y < rawHeight; ++y) {
            rowPointers[y] = (png_byte *)&pixels[4 * y * rawWidth];
        }
    }
    png_read_image(png, rowPointers);
    png_destroy_read_struct(&png, &info, NULL);
    clFree(rowPointers);
    return image;
}

struct writeInfo
{
    struct clContext * C;
    clRaw * dst;
    png_size_t offset;
};

static void writeCallback(png_structp png, png_bytep data, png_size_t length)
{
    struct writeInfo * wi = (struct writeInfo *)png_get_io_ptr(png);
    if ((wi->offset + length) > wi->dst->size) {
        size_t newSize = wi->dst->size;
        if (!newSize)
            newSize = 8;
        do {
            newSize *= 2;
        } while (newSize < (wi->offset + length));
        clRawRealloc(wi->C, wi->dst, newSize);
    }
    memcpy(wi->dst->ptr + wi->offset, data, length);
    wi->offset += length;
}

clBool clFormatWritePNG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(writeParams);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    COLORIST_ASSERT(png && info);

    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        return clFalse;
    }

    png_bytep * rowPointers = NULL;

    if (setjmp(png_jmpbuf(png))) {
        if (rowPointers) {
            clFree(rowPointers);
        }
        clRawFree(C, &rawProfile);
        png_destroy_write_struct(&png, &info);
        return clFalse;
    }

    struct writeInfo wi;
    wi.C = C;
    wi.offset = 0;
    wi.dst = output;
    png_set_write_fn(png, &wi, writeCallback, NULL);

    png_set_IHDR(
        png,
        info,
        image->width, image->height,
        image->depth,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
        );
    png_set_iCCP(png, info, image->profile->description, 0, rawProfile.ptr, (png_uint_32)rawProfile.size);
    png_write_info(png, info);

    rowPointers = (png_bytep *)clAllocate(sizeof(png_bytep) * image->height);
    int imgBytesPerChannel = (image->depth == 16) ? 2 : 1;
    if (imgBytesPerChannel == 1) {
        uint8_t * pixels = image->pixels;
        for (int y = 0; y < image->height; ++y) {
            rowPointers[y] = &pixels[4 * y * image->width];
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (int y = 0; y < image->height; ++y) {
            rowPointers[y] = (png_byte *)&pixels[4 * y * image->width];
        }
        png_set_swap(png);
    }

    png_write_image(png, rowPointers);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);

    clFree(rowPointers);
    clRawFree(C, &rawProfile);
    output->size = wi.offset;
    return clTrue;
}
