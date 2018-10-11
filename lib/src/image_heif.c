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

#include "libheif/heif.h"

// TODO: LOTS AND LOTS OF ERROR CHECKING -- THIS IS INCOMPLETE

clImage * clImageReadHEIF(struct clContext * C, const char * filename)
{
    clImage * image = NULL;
    clProfile * profile = NULL;
    clRaw rawProfile;
    size_t rawProfileSize = 0;

    struct heif_context * ctx = heif_context_alloc();
    struct heif_image_handle * heifHandle = NULL;
    struct heif_image * heifImage = NULL;
    const uint8_t * heifPixels = NULL;
    int heifWidth, heifHeight;
    int heifStride = 0;

    memset(&rawProfile, 0, sizeof(rawProfile));

    heif_context_read_from_file(ctx, filename, NULL);
    heif_context_get_primary_image_handle(ctx, &heifHandle);
    heif_decode_image(heifHandle, &heifImage, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, NULL);

    heifWidth = heif_image_get_width(heifImage, heif_channel_interleaved);
    heifHeight = heif_image_get_height(heifImage, heif_channel_interleaved);
    heifPixels = heif_image_get_plane_readonly(heifImage, heif_channel_interleaved, &heifStride);

    rawProfileSize = heif_image_handle_get_raw_color_profile_size(heifHandle);
    if (rawProfileSize > 0) {
        clRawRealloc(C, &rawProfile, rawProfileSize);
        heif_image_handle_get_raw_color_profile(heifHandle, rawProfile.ptr);
        profile = clProfileParse(C, rawProfile.ptr, rawProfile.size, NULL);
        if (profile == NULL) {
            clContextLogError(C, "Invalid ICC embedded profile: '%s'", filename);
            goto readCleanup;
        }
    }

    clImageLogCreate(C, heifWidth, heifHeight, 8, profile);
    image = clImageCreate(C, heifWidth, heifHeight, 8, profile);
    memcpy(image->pixels, heifPixels, heifStride * image->height);

readCleanup:
    clRawFree(C, &rawProfile);
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;
}

clBool clImageWriteHEIF(struct clContext * C, clImage * image, const char * filename, int quality)
{
    clBool writeResult = clTrue;
    clRaw rawProfile;
    int pixelCount = image->width * image->height;
    int channelCount = 4 * pixelCount;
    struct heif_context * ctx = heif_context_alloc();
    struct heif_encoder * encoder = NULL;
    struct heif_image * heifImage = NULL;
    int heifStride = 0;
    uint8_t * heifPixels;

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        goto writeCleanup;
    }

    // TODO: Remove this block when libheif's image class adds 64bit RGBA support.
    if (image->depth != 8) {
        clContextLogError(C, "HEIF writer can currently only handle 8 bit depth");
        goto writeCleanup;
    }

    if ((image->depth != 8) && (image->depth != 16)) {
        clContextLogError(C, "HEIF writer can currently only handle 8 and 16 bit depths");
        goto writeCleanup;
    }

    heif_context_get_encoder_for_format(ctx, heif_compression_HEVC, &encoder);
    heif_encoder_set_lossy_quality(encoder, quality);
    heif_image_create(image->width, image->height, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, &heifImage);
    heif_image_add_plane(heifImage, heif_channel_interleaved, image->width, image->height, 4 * image->depth);
    heif_image_set_raw_color_profile(heifImage, "prof", rawProfile.ptr, rawProfile.size);
    heifPixels = heif_image_get_plane(heifImage, heif_channel_interleaved, &heifStride);
    if (image->depth == 8) {
        memcpy(heifPixels, image->pixels, heifStride * image->height);
    } else {
        int index;
        uint16_t * srcPixels = (uint16_t *)image->pixels;
        uint16_t * dstPixels = (uint16_t *)heifPixels;
        for (index = 0; index < channelCount; ++index) {
            uint8_t * src = (uint8_t *)&srcPixels[index];
            uint8_t * dst = (uint8_t *)&dstPixels[index];
            dst[0] = src[0];
            dst[1] = src[1];
        }
    }
    heif_context_encode_image(ctx, heifImage, encoder, NULL, NULL);
    heif_context_write_to_file(ctx, filename);

writeCleanup:
    if (encoder)
        heif_encoder_release(encoder);
    if (heifImage)
        heif_image_release(heifImage);
    if (ctx)
        heif_context_free(ctx);

    clRawFree(C, &rawProfile);
    return writeResult;
}
