// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/profile.h"
#include "png.h"

#include <string.h>

clImage * clImageReadPNG(const char * filename)
{
    clImage * image;
    clProfile * profile = NULL;

    png_structp png;
    png_infop info;
    png_byte header[8];

    char * iccpProfileName;
    int iccpCompression;
    unsigned char * iccpData;
    png_uint_32 iccpDataLen;

    int rawWidth, rawHeight;
    png_byte rawColorType;
    png_byte rawBitDepth;

    int imgBitDepth = 8;
    int imgBytesPerChannel = 1;

    png_bytep * rowPointers;
    int y;

    FILE * fp = fopen(filename, "rb");
    if (!fp) {
        clLogError("cannot open PNG: '%s'", filename);
        return NULL;
    }

    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8)) {
        fclose(fp);
        clLogError("not a PNG: '%s'", filename);
        return NULL;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);
    setjmp(png_jmpbuf(png));
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    if (png_get_iCCP(png, info, &iccpProfileName, &iccpCompression, &iccpData, &iccpDataLen) == PNG_INFO_iCCP) {
        profile = clProfileParse(iccpData, iccpDataLen, iccpProfileName);
    }

    rawWidth = png_get_image_width(png, info);
    rawHeight = png_get_image_height(png, info);
    rawColorType = png_get_color_type(png, info);
    rawBitDepth = png_get_bit_depth(png, info);

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

    if (rawBitDepth == 16) {
        png_set_swap(png);
        imgBitDepth = 16;
        imgBytesPerChannel = 2;
    }

    png_read_update_info(png, info);
    setjmp(png_jmpbuf(png));

    image = clImageCreate(rawWidth, rawHeight, imgBitDepth, profile);
    if (profile) {
        clProfileDestroy(profile);
    }
    rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * rawHeight);
    if (imgBytesPerChannel == 1) {
        uint8_t * pixels = (uint8_t *)image->pixels;
        for (y = 0; y < rawHeight; ++y) {
            rowPointers[y] = &pixels[4 * y * rawWidth];
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (y = 0; y < rawHeight; ++y) {
            rowPointers[y] = (png_byte *)&pixels[4 * y * rawWidth];
        }
    }
    png_read_image(png, rowPointers);
    png_destroy_read_struct(&png, &info, NULL);
    free(rowPointers);
    fclose(fp);
    return image;
}

clBool clImageWritePNG(clImage * image, const char * filename)
{
    int y;
    png_bytep * rowPointers;
    int imgBytesPerChannel = (image->depth == 16) ? 2 : 1;
    clRaw rawProfile;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);

    FILE * fp = fopen(filename, "wb");
    if (!fp) {
        return clFalse;
    }

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(image->profile, &rawProfile)) {
        fclose(fp);
        return clFalse;
    }

    COLORIST_ASSERT(png && info);
    setjmp(png_jmpbuf(png));
    png_init_io(png, fp);

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
    png_set_iCCP(png, info, image->profile->description, 0, rawProfile.ptr, rawProfile.size);
    png_write_info(png, info);

    rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * image->height);
    if (imgBytesPerChannel == 1) {
        uint8_t * pixels = (uint8_t *)image->pixels;
        for (y = 0; y < image->height; ++y) {
            rowPointers[y] = &pixels[4 * y * image->width];
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (y = 0; y < image->height; ++y) {
            rowPointers[y] = (png_byte *)&pixels[4 * y * image->width];
        }
        png_set_swap(png);
    }

    png_write_image(png, rowPointers);
    png_write_end(png, NULL);

    free(rowPointers);
    fclose(fp);
    clRawFree(&rawProfile);
    return clTrue;
}
