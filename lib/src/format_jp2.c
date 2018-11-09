// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include "openjpeg.h"
#include "opj_malloc.h"

#include <string.h>

struct clImage * clFormatReadJP2(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteJP2(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

static void error_callback(const char * msg, void * client_data)
{
    COLORIST_UNUSED(client_data);

    clContext * C = (clContext *)client_data;
    clContextLogError(C, "%s", msg);
}
static void warning_callback(const char * msg, void * client_data)
{
    COLORIST_UNUSED(msg);
    COLORIST_UNUSED(client_data);
}
static void info_callback(const char * msg, void * client_data)
{
    COLORIST_UNUSED(msg);
    COLORIST_UNUSED(client_data);
}

struct opjCallbackInfo
{
    struct clContext * C;
    clRaw * raw;
    OPJ_OFF_T offset;
};

static OPJ_SIZE_T readCallback(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data)
{
    struct opjCallbackInfo * ci = (struct opjCallbackInfo *)p_user_data;
    if ((ci->offset + p_nb_bytes) > ci->raw->size) {
        p_nb_bytes = (OPJ_SIZE_T)(ci->raw->size - ci->offset);
    }
    memcpy(p_buffer, ci->raw->ptr + ci->offset, p_nb_bytes);
    ci->offset += p_nb_bytes;
    return p_nb_bytes;
}

static OPJ_SIZE_T writeCallback(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data)
{
    struct opjCallbackInfo * ci = (struct opjCallbackInfo *)p_user_data;
    if ((ci->offset + p_nb_bytes) > ci->raw->size) {
        size_t newSize = (size_t)(ci->offset + p_nb_bytes);
        clRawRealloc(ci->C, ci->raw, newSize);
    }
    memcpy(ci->raw->ptr + ci->offset, p_buffer, p_nb_bytes);
    ci->offset += p_nb_bytes;
    return p_nb_bytes;
}

static OPJ_OFF_T skipCallback(OPJ_OFF_T p_nb_bytes, void * p_user_data)
{
    struct opjCallbackInfo * ci = (struct opjCallbackInfo *)p_user_data;
    ci->offset += (uint32_t)p_nb_bytes;
    return p_nb_bytes;
}

static OPJ_BOOL seekCallback(OPJ_OFF_T p_nb_bytes, void * p_user_data)
{
    struct opjCallbackInfo * ci = (struct opjCallbackInfo *)p_user_data;
    ci->offset = (uint32_t)p_nb_bytes;
    return OPJ_TRUE;
}

struct clImage * clFormatReadJP2(struct clContext * C, const char * formatName, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);

    clImage * image = NULL;
    clProfile * profile = NULL;
    int i, pixelCount, dstDepth;

    opj_dparameters_t parameters;
    opj_codec_t * opjCodec = NULL;
    opj_image_t * opjImage = NULL;
    opj_stream_t * opjStream = NULL;
    int channelFactor[4] = { 1, 1, 1, 1 };
    int maxChannel;
    struct opjCallbackInfo ci;

    const char * errorExtName = "JP2";
    clBool isJ2K = clFalse;
    if (input->size < 4) {
        clContextLogError(C, "JP2/J2K header too small");
        return NULL;
    }

    static const unsigned char j2kHeader[4] = { 0xff, 0x4f, 0xff, 0x51 };
    if (!memcmp(input->ptr, j2kHeader, 4)) {
        isJ2K = clTrue;
        errorExtName = "J2K";
    }

    ci.C = C;
    ci.raw = input;
    ci.offset = 0;

    opjStream = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_TRUE);
    opj_stream_set_user_data(opjStream, &ci, NULL);
    opj_stream_set_user_data_length(opjStream, input->size);
    opj_stream_set_read_function(opjStream, readCallback);
    opj_stream_set_skip_function(opjStream, skipCallback);
    opj_stream_set_seek_function(opjStream, seekCallback);

    opjCodec = opj_create_decompress(isJ2K ? OPJ_CODEC_J2K : OPJ_CODEC_JP2);
    opj_set_info_handler(opjCodec, info_callback, C);
    opj_set_warning_handler(opjCodec, warning_callback, C);
    opj_set_error_handler(opjCodec, error_callback, C);
    opj_set_default_decoder_parameters(&parameters);

    if (!opj_setup_decoder(opjCodec, &parameters) ) {
        clContextLogError(C, "Failed to setup %s decoder", errorExtName);
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        return NULL;
    }

    if (!opj_read_header(opjStream, opjCodec, &opjImage)) {
        clContextLogError(C, "Failed to read %s header", errorExtName);
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        return NULL;
    }

    if (!opj_decode(opjCodec, opjStream, opjImage)) {
        clContextLogError(C, "Failed to decode %s!", errorExtName);
        opj_destroy_codec(opjCodec);
        opj_stream_destroy(opjStream);
        opj_image_destroy(opjImage);
        return NULL;
    }

    if ((opjImage->numcomps != 3) && (opjImage->numcomps != 4)) {
        clContextLogError(C, "Unsupported %s component count: %d", errorExtName, opjImage->numcomps);
        opj_destroy_codec(opjCodec);
        opj_stream_destroy(opjStream);
        opj_image_destroy(opjImage);
        return NULL;
    }

    if (opjImage->icc_profile_buf && (opjImage->icc_profile_len > 0)) {
        profile = clProfileParse(C, opjImage->icc_profile_buf, opjImage->icc_profile_len, NULL);
    }

    dstDepth = 8;
    for (i = 0; i < (int)opjImage->numcomps; ++i) {
        // Find biggest component
        dstDepth = (dstDepth > (int)opjImage->comps[i].prec) ? dstDepth : (int)opjImage->comps[i].prec;
    }
    if ((dstDepth < 8) || (dstDepth > 16)) {
        int srcDepth = dstDepth;
        dstDepth = CL_CLAMP(dstDepth, 8, 16); // round to nearest Colorist-supported depth
        clContextLog(C, "JP2", 1, "Clamping %d-bit source to %d bits", srcDepth, dstDepth);
    }
    for (i = 0; i < (int)opjImage->numcomps; ++i) {
        // Calculate scales for incoming components
        channelFactor[i] = 1 << (dstDepth - opjImage->comps[i].prec);
    }
    maxChannel = (1 << dstDepth) - 1;

    clImageLogCreate(C, opjImage->x1, opjImage->y1, dstDepth, profile);
    image = clImageCreate(C, opjImage->x1, opjImage->y1, dstDepth, profile);
    if (profile) {
        clProfileDestroy(C, profile);
    }

    pixelCount = image->width * image->height;

    if (image->depth > 8) {
        uint16_t * pixel = (uint16_t *)image->pixels;
        if (opjImage->numcomps == 3) {
            // RGB, fill A
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = (uint16_t)(opjImage->comps[0].data[i] * channelFactor[0]);
                pixel[1] = (uint16_t)(opjImage->comps[1].data[i] * channelFactor[1]);
                pixel[2] = (uint16_t)(opjImage->comps[2].data[i] * channelFactor[2]);
                pixel[3] = (uint16_t)(maxChannel);
                pixel += 4;
            }
        } else {
            // RGBA
            COLORIST_ASSERT(opjImage->numcomps == 4);
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = (uint16_t)(opjImage->comps[0].data[i] * channelFactor[0]);
                pixel[1] = (uint16_t)(opjImage->comps[1].data[i] * channelFactor[1]);
                pixel[2] = (uint16_t)(opjImage->comps[2].data[i] * channelFactor[2]);
                pixel[3] = (uint16_t)(opjImage->comps[3].data[i] * channelFactor[3]);
                pixel += 4;
            }
        }
    } else {
        uint8_t * pixel = image->pixels;
        if (opjImage->numcomps == 3) {
            // RGB, fill A
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = (uint8_t)(opjImage->comps[0].data[i] * channelFactor[0]);
                pixel[1] = (uint8_t)(opjImage->comps[1].data[i] * channelFactor[1]);
                pixel[2] = (uint8_t)(opjImage->comps[2].data[i] * channelFactor[2]);
                pixel[3] = (uint8_t)(maxChannel);
                pixel += 4;
            }
        } else {
            // RGBA
            COLORIST_ASSERT(opjImage->numcomps == 4);
            for (i = 0; i < pixelCount; ++i) {
                pixel[0] = (uint8_t)(opjImage->comps[0].data[i] * channelFactor[0]);
                pixel[1] = (uint8_t)(opjImage->comps[1].data[i] * channelFactor[1]);
                pixel[2] = (uint8_t)(opjImage->comps[2].data[i] * channelFactor[2]);
                pixel[3] = (uint8_t)(opjImage->comps[3].data[i] * channelFactor[3]);
                pixel += 4;
            }
        }
    }

    opj_stream_destroy(opjStream);
    opj_destroy_codec(opjCodec);
    opj_image_destroy(opjImage);

    return image;
}

clBool clFormatWriteJP2(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
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
    clBool isJ2K = !strcmp(formatName, "j2k");
    struct opjCallbackInfo ci;

    ci.C = C;
    ci.raw = output;
    ci.offset = 0;

    opjStream = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_FALSE);
    opj_stream_set_user_data(opjStream, &ci, NULL);
    opj_stream_set_write_function(opjStream, writeCallback);
    opj_stream_set_skip_function(opjStream, skipCallback);
    opj_stream_set_seek_function(opjStream, seekCallback);

    memset(&parameters, 0, sizeof(parameters));
    opj_set_default_encoder_parameters(&parameters);
    parameters.cod_format = 0;
    parameters.tcp_numlayers = 1;
    parameters.cp_disto_alloc = 1;
    parameters.tcp_mct = 1;
    parameters.numresolution = 1;
    while (parameters.numresolution < 6) {
        if (image->width <= (1 << (parameters.numresolution - 1)))
            break;
        if (image->height <= (1 << (parameters.numresolution - 1)))
            break;
        ++parameters.numresolution;
    }

    if (writeParams->rate != 0) {
        parameters.tcp_rates[0] = (float)writeParams->rate;
    } else {
        if ((writeParams->quality == 0) || (writeParams->quality == 100)) {
            // Lossless
            parameters.tcp_rates[0] = 0; // lossless
        } else {
            // Set quality
            parameters.tcp_distoratio[0] = (float)writeParams->quality;
            parameters.cp_fixed_quality = OPJ_TRUE;
        }
    }

    for (i = 0; i < numcomps; i++) {
        memset(&cmptparm[i], 0, sizeof(cmptparm[0]));
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

    if (image->depth > 8) {
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
    opjImage->comps[3].alpha = 1;

    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        return clFalse;
    }
    opjImage->icc_profile_buf = opj_malloc(rawProfile.size);
    memcpy(opjImage->icc_profile_buf, rawProfile.ptr, rawProfile.size);
    opjImage->icc_profile_len = (OPJ_UINT32)rawProfile.size;

    opj_set_info_handler(opjCodec, info_callback, C);
    opj_set_warning_handler(opjCodec, warning_callback, C);
    opj_set_error_handler(opjCodec, error_callback, C);
    opjCodec = opj_create_compress(isJ2K ? OPJ_CODEC_J2K : OPJ_CODEC_JP2);
    opj_set_info_handler(opjCodec, info_callback, C);
    opj_set_warning_handler(opjCodec, warning_callback, C);
    opj_set_error_handler(opjCodec, error_callback, C);

    opj_setup_encoder(opjCodec, &parameters, opjImage);

    bSuccess = opj_start_compress(opjCodec, opjImage, opjStream);
    if (!bSuccess) {
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        clRawFree(C, &rawProfile);
        return clFalse;
    }

    bSuccess = opj_encode(opjCodec, opjStream);
    if (!bSuccess) {
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        clRawFree(C, &rawProfile);
        return clFalse;
    }

    bSuccess = opj_end_compress(opjCodec, opjStream);
    if (!bSuccess) {
        opj_stream_destroy(opjStream);
        opj_destroy_codec(opjCodec);
        opj_image_destroy(opjImage);
        clRawFree(C, &rawProfile);
        return clFalse;
    }

    opj_stream_destroy(opjStream);
    opj_destroy_codec(opjCodec);
    opj_image_destroy(opjImage);
    clRawFree(C, &rawProfile);
    return clTrue;
}
