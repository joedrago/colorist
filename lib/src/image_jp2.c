// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/profile.h"
#include "openjpeg.h"
#include "opj_malloc.h"

#include <string.h>

static void error_callback(const char * msg, void * client_data)
{
    (void)client_data;
    printf("[ERROR] %s", msg);
}
static void warning_callback(const char * msg, void * client_data)
{
    (void)client_data;
    // printf("[WARNING] %s", msg);
}
static void info_callback(const char * msg, void * client_data)
{
    (void)client_data;
    // printf("[INFO] %s", msg);
}

clImage * clImageReadJP2(const char * filename)
{
    clImage * image = NULL;
    clProfile * profile = NULL;
    int i, pixelCount;

    opj_dparameters_t parameters;
    opj_codec_t * opjCodec = NULL;
    opj_image_t * opjImage = NULL;
    opj_stream_t * opjStream = NULL;

    // TODO: detect J2K stream here?
    opjCodec = opj_create_decompress(OPJ_CODEC_JP2);
    opj_set_info_handler(opjCodec, info_callback, 00);
    opj_set_warning_handler(opjCodec, warning_callback, 00);
    opj_set_error_handler(opjCodec, error_callback, 00);

    opj_set_default_decoder_parameters(&parameters);

    opjStream = opj_stream_create_file_stream(filename, 1 * 1024 * 1024, OPJ_TRUE);

    /* Setup the decoder decoding parameters using user parameters */
    if (!opj_setup_decoder(opjCodec, &parameters) ) {
        fprintf(stderr, "ERROR: Failed to setup JP2 decoder\n");
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        return NULL;
    }

    /* Read the main header of the codestream and if necessary the JP2 boxes*/
    if (!opj_read_header(opjStream, opjCodec, &opjImage)) {
        fprintf(stderr, "ERROR: Failed to read JP2 header\n");
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        return NULL;
    }

    /* decode the image */
    if (!opj_decode(opjCodec, opjStream, opjImage)) {
        fprintf(stderr, "ERROR: Failed to decode JP2!\n");
        opj_destroy_codec(opjCodec);
        opj_stream_destroy(opjStream);
        opj_image_destroy(opjImage);
        return NULL;
    }

    if ((opjImage->numcomps != 3) && (opjImage->numcomps != 4)) {
        fprintf(stderr, "ERROR: Unsupported JP2 component count: %d\n", opjImage->numcomps);
        opj_destroy_codec(opjCodec);
        opj_stream_destroy(opjStream);
        opj_image_destroy(opjImage);
        return NULL;
    }

    if (opjImage->icc_profile_buf && (opjImage->icc_profile_len > 0)) {
        profile = clProfileParse(opjImage->icc_profile_buf, opjImage->icc_profile_len, NULL);
        if (profile) {
            char * description = clProfileGetMLU(profile, "desc", "en", "US");
            COLORIST_ASSERT(!profile->description);
            if (description) {
                profile->description = strdup(description);
            } else {
                profile->description = strdup("Unknown");
            }
        }
    }

    image = clImageCreate(opjImage->x1, opjImage->y1, opjImage->comps[0].prec, profile);
    clProfileDestroy(profile);

    pixelCount = image->width * image->height;

    if (image->depth == 16) {
        // 16-bit
        uint16_t * pixel = (uint16_t *)image->pixels;
        if (opjImage->numcomps == 3) {
            // RGB, fill A
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = opjImage->comps[0].data[i];
                pixel[1] = opjImage->comps[1].data[i];
                pixel[2] = opjImage->comps[2].data[i];
                pixel[3] = 65535;
                pixel += 4;
            }
        } else {
            // RGBA
            COLORIST_ASSERT(opjImage->numcomps == 4);
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = opjImage->comps[0].data[i];
                pixel[1] = opjImage->comps[1].data[i];
                pixel[2] = opjImage->comps[2].data[i];
                pixel[3] = opjImage->comps[3].data[i];
                pixel += 4;
            }
        }
    } else {
        // 8-bit
        uint8_t * pixel = image->pixels;
        if (opjImage->numcomps == 3) {
            // RGB, fill A
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = opjImage->comps[0].data[i];
                pixel[1] = opjImage->comps[1].data[i];
                pixel[2] = opjImage->comps[2].data[i];
                pixel[3] = 255;
                pixel += 4;
            }
        } else {
            // RGBA
            COLORIST_ASSERT(opjImage->numcomps == 4);
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = opjImage->comps[0].data[i];
                pixel[1] = opjImage->comps[1].data[i];
                pixel[2] = opjImage->comps[2].data[i];
                pixel[3] = opjImage->comps[3].data[i];
                pixel += 4;
            }
        }
    }

    opj_stream_destroy(opjStream);
    opj_destroy_codec(opjCodec);
    opj_image_destroy(opjImage);
    return image;
}

clBool clImageWriteJP2(clImage * image, const char * filename, int quality, int rate)
{
    const OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_SRGB;
    int numcomps = 4;
    int i, j;
    opj_cparameters_t parameters;
    unsigned int subsampling_dx = 1;
    unsigned int subsampling_dy = 1;
    opj_image_cmptparm_t cmptparm[4];
    opj_image_t * opjImage;
    opj_codec_t * opjCodec = NULL;
    OPJ_BOOL bSuccess;
    opj_stream_t * opjStream = NULL;
    clRaw rawProfile;

    opjStream = opj_stream_create_file_stream(filename, 1 * 1024 * 1024, OPJ_FALSE);

    opj_set_default_encoder_parameters(&parameters);
    parameters.cod_format = 0;
    parameters.tcp_numlayers = 1;
    parameters.cp_disto_alloc = 1;

    if (rate != 0) {
        parameters.tcp_rates[0] = (float)rate;
    } else {
        if ((quality == 0) || (quality == 100)) {
            // Lossless
            parameters.tcp_rates[0] = 0; // lossless
        } else {
            // Set quality
            parameters.tcp_distoratio[0] = (float)quality;
            parameters.cp_fixed_quality = OPJ_TRUE;
        }
    }

    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = image->depth;
        cmptparm[i].bpp = image->depth;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = subsampling_dx;
        cmptparm[i].dy = subsampling_dy;
        cmptparm[i].w = image->width;
        cmptparm[i].h = image->height;
    }

    opjImage = opj_image_create(numcomps, cmptparm, color_space);
    if (!opjImage) {
        return 0;
    }

    if (image->depth == 16) {
        unsigned short * src = (unsigned short *)image->pixels;
        for (j = 0; j < image->height; ++j) {
            for (i = 0; i < image->width; ++i) {
                int dstOffset = i + (j * image->width);
                int srcOffset = 4 * dstOffset;
                opjImage->comps[0].data[dstOffset] = src[srcOffset + 0];
                opjImage->comps[1].data[dstOffset] = src[srcOffset + 1];
                opjImage->comps[2].data[dstOffset] = src[srcOffset + 2];
                opjImage->comps[3].data[dstOffset] = src[srcOffset + 3];
            }
        }
    } else {
        unsigned char * src = (unsigned char *)image->pixels;
        for (j = 0; j < image->height; ++j) {
            for (i = 0; i < image->width; ++i) {
                int dstOffset = i + (j * image->width);
                int srcOffset = 4 * dstOffset;
                opjImage->comps[0].data[dstOffset] = src[srcOffset + 0];
                opjImage->comps[1].data[dstOffset] = src[srcOffset + 1];
                opjImage->comps[2].data[dstOffset] = src[srcOffset + 2];
                opjImage->comps[3].data[dstOffset] = src[srcOffset + 3];
            }
        }
    }

    opjImage->x0 = 0;
    opjImage->y0 = 0;
    opjImage->x1 = image->width;
    opjImage->y1 = image->height;

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(image->profile, &rawProfile)) {
        return clFalse;
    }
    opjImage->icc_profile_buf = opj_malloc(rawProfile.size);
    memcpy(opjImage->icc_profile_buf, rawProfile.ptr, rawProfile.size);
    opjImage->icc_profile_len = rawProfile.size;

    opj_set_info_handler(opjCodec, info_callback, 00);
    opj_set_warning_handler(opjCodec, warning_callback, 00);
    opj_set_error_handler(opjCodec, error_callback, 00);
    opjCodec = opj_create_compress(OPJ_CODEC_JP2);
    opj_set_info_handler(opjCodec, info_callback, 00);
    opj_set_warning_handler(opjCodec, warning_callback, 00);
    opj_set_error_handler(opjCodec, error_callback, 00);

    opj_setup_encoder(opjCodec, &parameters, opjImage);

    bSuccess = opj_start_compress(opjCodec, opjImage, opjStream);
    if (!bSuccess) {
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        clRawFree(&rawProfile);
        return clFalse;
    }

    bSuccess = opj_encode(opjCodec, opjStream);
    if (!bSuccess) {
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        clRawFree(&rawProfile);
        return clFalse;
    }

    bSuccess = opj_end_compress(opjCodec, opjStream);
    if (!bSuccess) {
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        clRawFree(&rawProfile);
        return clFalse;
    }

    opj_stream_destroy(opjStream);
    opj_destroy_codec(opjCodec);
    opj_image_destroy(opjImage);
    clRawFree(&rawProfile);
    return clTrue;
}
