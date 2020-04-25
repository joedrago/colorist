// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"

#include "openjpeg.h"
#include "opj_malloc.h"

#include <string.h>

extern void color_sycc_to_rgb(opj_image_t * img);
extern void color_cmyk_to_rgb(opj_image_t * image);
extern void color_esycc_to_rgb(opj_image_t * image);

struct clImage * clFormatReadJP2(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
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

static int limitedToFullY(int depth, int v)
{
    switch (depth) {
        case 8:
            v = ((v - 16) * 255) / (235 - 16);
            v = CL_CLAMP(v, 0, 255);
            return v;
        case 10:
            v = ((v - 64) * 1023) / (940 - 64);
            v = CL_CLAMP(v, 0, 1023);
            return v;
        case 12:
            v = ((v - 256) * 4095) / (3760 - 256);
            v = CL_CLAMP(v, 0, 4095);
            return v;
    }
    return v;
}

static int limitedToFullUV(int depth, int v)
{
    switch (depth) {
        case 8:
            v = ((v - 16) * 255) / (240 - 16);
            v = CL_CLAMP(v, 0, 255);
            return v;
        case 10:
            v = ((v - 64) * 1023) / (960 - 64);
            v = CL_CLAMP(v, 0, 1023);
            return v;
        case 12:
            v = ((v - 256) * 4095) / (3840 - 256);
            v = CL_CLAMP(v, 0, 4095);
            return v;
    }
    return v;
}

struct clImage * clFormatReadJP2(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input)
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

    Timer t;
    timerStart(&t);

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

    if (!opj_setup_decoder(opjCodec, &parameters)) {
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

    C->readExtraInfo.decodeCodecSeconds = timerElapsedSeconds(&t);

    if ((opjImage->numcomps != 3) && (opjImage->numcomps != 4)) {
        clContextLogError(C, "Unsupported %s component count: %d", errorExtName, opjImage->numcomps);
        opj_destroy_codec(opjCodec);
        opj_stream_destroy(opjStream);
        opj_image_destroy(opjImage);
        return NULL;
    }

    if (overrideProfile) {
        profile = clProfileClone(C, overrideProfile);
    } else if (opjImage->icc_profile_buf && (opjImage->icc_profile_len > 0)) {
        profile = clProfileParse(C, opjImage->icc_profile_buf, opjImage->icc_profile_len, NULL);
    }

    int chromaShiftX = 0;
    int chromaShiftY = 0;

    dstDepth = 8;
    for (i = 0; i < (int)opjImage->numcomps; ++i) {
        // Find biggest component
        dstDepth = (dstDepth > (int)opjImage->comps[i].prec) ? dstDepth : (int)opjImage->comps[i].prec;

        // Check for subsampling
        if (opjImage->comps[i].dx == 2) {
            chromaShiftX = 1;
        }
        if (opjImage->comps[i].dy == 2) {
            chromaShiftY = 1;
        }
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

    if (opjImage->color_space != OPJ_CLRSPC_SYCC && opjImage->numcomps == 3 && opjImage->comps[0].dx == opjImage->comps[0].dy &&
        opjImage->comps[1].dx != 1) {
        opjImage->color_space = OPJ_CLRSPC_SYCC;
    } else if (opjImage->numcomps <= 2) {
        opjImage->color_space = OPJ_CLRSPC_GRAY;
    }

    if (opjImage->color_space == OPJ_CLRSPC_SYCC) {
        color_sycc_to_rgb(opjImage);
    } else if (opjImage->color_space == OPJ_CLRSPC_CMYK) {
        color_cmyk_to_rgb(opjImage);
    } else if (opjImage->color_space == OPJ_CLRSPC_EYCC) {
        color_esycc_to_rgb(opjImage);
    }

    clImageLogCreate(C, opjImage->x1, opjImage->y1, dstDepth, profile);
    image = clImageCreate(C, opjImage->x1, opjImage->y1, dstDepth, profile);
    if (profile) {
        clProfileDestroy(C, profile);
    }
    clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U16);

    pixelCount = image->width * image->height;

    timerStart(&t);

    uint16_t * pixel = image->pixelsU16;
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
    C->readExtraInfo.decodeFillSeconds = timerElapsedSeconds(&t);

    opj_stream_destroy(opjStream);
    opj_destroy_codec(opjCodec);
    opj_image_destroy(opjImage);

    return image;
}

clBool clFormatWriteJP2(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    struct opjCallbackInfo ci;
    ci.C = C;
    ci.raw = output;
    ci.offset = 0;

    opj_stream_t * opjStream = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_FALSE);
    opj_stream_set_user_data(opjStream, &ci, NULL);
    opj_stream_set_write_function(opjStream, writeCallback);
    opj_stream_set_skip_function(opjStream, skipCallback);
    opj_stream_set_seek_function(opjStream, seekCallback);

    opj_cparameters_t parameters;
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

    int numcomps = 4;
    opj_image_cmptparm_t cmptparm[4];
    unsigned int subsampling_dx = 1;
    unsigned int subsampling_dy = 1;
    for (int i = 0; i < numcomps; i++) {
        memset(&cmptparm[i], 0, sizeof(cmptparm[0]));
        cmptparm[i].prec = image->depth;
        cmptparm[i].bpp = image->depth;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = subsampling_dx;
        cmptparm[i].dy = subsampling_dy;
        cmptparm[i].w = image->width;
        cmptparm[i].h = image->height;
    }

    const OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_SRGB;
    opj_image_t * opjImage = opj_image_create(numcomps, cmptparm, color_space);
    if (!opjImage) {
        return 0;
    }

    clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_U16);
    for (int j = 0; j < image->height; ++j) {
        for (int i = 0; i < image->width; ++i) {
            int dstOffset = i + (j * image->width);
            int srcOffset = 4 * dstOffset;
            opjImage->comps[0].data[dstOffset] = image->pixelsU16[srcOffset + 0];
            opjImage->comps[1].data[dstOffset] = image->pixelsU16[srcOffset + 1];
            opjImage->comps[2].data[dstOffset] = image->pixelsU16[srcOffset + 2];
            opjImage->comps[3].data[dstOffset] = image->pixelsU16[srcOffset + 3];
        }
    }

    opjImage->x0 = 0;
    opjImage->y0 = 0;
    opjImage->x1 = image->width;
    opjImage->y1 = image->height;
    opjImage->comps[3].alpha = 1;

    clRaw rawProfile = CL_RAW_EMPTY;
    if (writeParams->writeProfile) {
        if (!clProfilePack(C, image->profile, &rawProfile)) {
            return clFalse;
        }
        opjImage->icc_profile_buf = opj_malloc(rawProfile.size);
        memcpy(opjImage->icc_profile_buf, rawProfile.ptr, rawProfile.size);
        opjImage->icc_profile_len = (OPJ_UINT32)rawProfile.size;
    } else {
        opjImage->icc_profile_buf = NULL;
        opjImage->icc_profile_len = 0;
    }

    opj_codec_t * opjCodec = NULL;
    clBool isJ2K = !strcmp(formatName, "j2k");
    opj_set_info_handler(opjCodec, info_callback, C);
    opj_set_warning_handler(opjCodec, warning_callback, C);
    opj_set_error_handler(opjCodec, error_callback, C);
    opjCodec = opj_create_compress(isJ2K ? OPJ_CODEC_J2K : OPJ_CODEC_JP2);
    opj_set_info_handler(opjCodec, info_callback, C);
    opj_set_warning_handler(opjCodec, warning_callback, C);
    opj_set_error_handler(opjCodec, error_callback, C);

    opj_setup_encoder(opjCodec, &parameters, opjImage);

    OPJ_BOOL bSuccess;

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

// From openjpeg's color.c:

/*--------------------------------------------------------
Matrix for sYCC, Amendment 1 to IEC 61966-2-1

Y :   0.299   0.587    0.114   :R
Cb:  -0.1687 -0.3312   0.5     :G
Cr:   0.5    -0.4187  -0.0812  :B

Inverse:

R: 1        -3.68213e-05    1.40199      :Y
G: 1.00003  -0.344125      -0.714128     :Cb - 2^(prec - 1)
B: 0.999823  1.77204       -8.04142e-06  :Cr - 2^(prec - 1)

-----------------------------------------------------------*/
static void sycc_to_rgb(int offset, int upb, int y, int cb, int cr, int * out_r, int * out_g, int * out_b)
{
    int r, g, b;

    cb -= offset;
    cr -= offset;
    r = y + (int)(1.402 * (float)cr);
    if (r < 0) {
        r = 0;
    } else if (r > upb) {
        r = upb;
    }
    *out_r = r;

    g = y - (int)(0.344 * (float)cb + 0.714 * (float)cr);
    if (g < 0) {
        g = 0;
    } else if (g > upb) {
        g = upb;
    }
    *out_g = g;

    b = y + (int)(1.772 * (float)cb);
    if (b < 0) {
        b = 0;
    } else if (b > upb) {
        b = upb;
    }
    *out_b = b;
}

static void sycc444_to_rgb(opj_image_t * img)
{
    int *d0, *d1, *d2, *r, *g, *b;
    const int *y, *cb, *cr;
    size_t maxw, maxh, max, i;
    int offset, upb;

    upb = (int)img->comps[0].prec;
    offset = 1 << (upb - 1);
    upb = (1 << upb) - 1;

    maxw = (size_t)img->comps[0].w;
    maxh = (size_t)img->comps[0].h;
    max = maxw * maxh;

    y = img->comps[0].data;
    cb = img->comps[1].data;
    cr = img->comps[2].data;

    d0 = r = (int *)opj_image_data_alloc(sizeof(int) * max);
    d1 = g = (int *)opj_image_data_alloc(sizeof(int) * max);
    d2 = b = (int *)opj_image_data_alloc(sizeof(int) * max);

    if (r == NULL || g == NULL || b == NULL) {
        goto fails;
    }

    for (i = 0U; i < max; ++i) {
        sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
        ++y;
        ++cb;
        ++cr;
        ++r;
        ++g;
        ++b;
    }
    opj_image_data_free(img->comps[0].data);
    img->comps[0].data = d0;
    opj_image_data_free(img->comps[1].data);
    img->comps[1].data = d1;
    opj_image_data_free(img->comps[2].data);
    img->comps[2].data = d2;
    img->color_space = OPJ_CLRSPC_SRGB;
    return;

fails:
    opj_image_data_free(r);
    opj_image_data_free(g);
    opj_image_data_free(b);
} /* sycc444_to_rgb() */

static void sycc422_to_rgb(opj_image_t * img)
{
    int *d0, *d1, *d2, *r, *g, *b;
    const int *y, *cb, *cr;
    size_t maxw, maxh, max, offx, loopmaxw;
    int offset, upb;
    size_t i;

    upb = (int)img->comps[0].prec;
    offset = 1 << (upb - 1);
    upb = (1 << upb) - 1;

    maxw = (size_t)img->comps[0].w;
    maxh = (size_t)img->comps[0].h;
    max = maxw * maxh;

    y = img->comps[0].data;
    cb = img->comps[1].data;
    cr = img->comps[2].data;

    d0 = r = (int *)opj_image_data_alloc(sizeof(int) * max);
    d1 = g = (int *)opj_image_data_alloc(sizeof(int) * max);
    d2 = b = (int *)opj_image_data_alloc(sizeof(int) * max);

    if (r == NULL || g == NULL || b == NULL) {
        goto fails;
    }

    /* if img->x0 is odd, then first column shall use Cb/Cr = 0 */
    offx = img->x0 & 1U;
    loopmaxw = maxw - offx;

    for (i = 0U; i < maxh; ++i) {
        size_t j;

        if (offx > 0U) {
            sycc_to_rgb(offset, upb, *y, 0, 0, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
        }

        for (j = 0U; j < (loopmaxw & ~(size_t)1U); j += 2U) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            ++cb;
            ++cr;
        }
        if (j < loopmaxw) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            ++cb;
            ++cr;
        }
    }

    opj_image_data_free(img->comps[0].data);
    img->comps[0].data = d0;
    opj_image_data_free(img->comps[1].data);
    img->comps[1].data = d1;
    opj_image_data_free(img->comps[2].data);
    img->comps[2].data = d2;

    img->comps[1].w = img->comps[2].w = img->comps[0].w;
    img->comps[1].h = img->comps[2].h = img->comps[0].h;
    img->comps[1].dx = img->comps[2].dx = img->comps[0].dx;
    img->comps[1].dy = img->comps[2].dy = img->comps[0].dy;
    img->color_space = OPJ_CLRSPC_SRGB;
    return;

fails:
    opj_image_data_free(r);
    opj_image_data_free(g);
    opj_image_data_free(b);
} /* sycc422_to_rgb() */

static void sycc420_to_rgb(opj_image_t * img)
{
    int *d0, *d1, *d2, *r, *g, *b, *nr, *ng, *nb;
    const int *y, *cb, *cr, *ny;
    size_t maxw, maxh, max, offx, loopmaxw, offy, loopmaxh;
    int offset, upb;
    size_t i;

    upb = (int)img->comps[0].prec;
    offset = 1 << (upb - 1);
    upb = (1 << upb) - 1;

    maxw = (size_t)img->comps[0].w;
    maxh = (size_t)img->comps[0].h;
    max = maxw * maxh;

    y = img->comps[0].data;
    cb = img->comps[1].data;
    cr = img->comps[2].data;

    d0 = r = (int *)opj_image_data_alloc(sizeof(int) * max);
    d1 = g = (int *)opj_image_data_alloc(sizeof(int) * max);
    d2 = b = (int *)opj_image_data_alloc(sizeof(int) * max);

    if (r == NULL || g == NULL || b == NULL) {
        goto fails;
    }

    /* if img->x0 is odd, then first column shall use Cb/Cr = 0 */
    offx = img->x0 & 1U;
    loopmaxw = maxw - offx;
    /* if img->y0 is odd, then first line shall use Cb/Cr = 0 */
    offy = img->y0 & 1U;
    loopmaxh = maxh - offy;

    if (offy > 0U) {
        size_t j;

        for (j = 0; j < maxw; ++j) {
            sycc_to_rgb(offset, upb, *y, 0, 0, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
        }
    }

    for (i = 0U; i < (loopmaxh & ~(size_t)1U); i += 2U) {
        size_t j;

        ny = y + maxw;
        nr = r + maxw;
        ng = g + maxw;
        nb = b + maxw;

        if (offx > 0U) {
            sycc_to_rgb(offset, upb, *y, 0, 0, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
        }

        for (j = 0; j < (loopmaxw & ~(size_t)1U); j += 2U) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;

            sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            ++cb;
            ++cr;
        }
        if (j < loopmaxw) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;

            sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            ++cb;
            ++cr;
        }
        y += maxw;
        r += maxw;
        g += maxw;
        b += maxw;
    }
    if (i < loopmaxh) {
        size_t j;

        for (j = 0U; j < (maxw & ~(size_t)1U); j += 2U) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);

            ++y;
            ++r;
            ++g;
            ++b;

            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);

            ++y;
            ++r;
            ++g;
            ++b;
            ++cb;
            ++cr;
        }
        if (j < maxw) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
        }
    }

    opj_image_data_free(img->comps[0].data);
    img->comps[0].data = d0;
    opj_image_data_free(img->comps[1].data);
    img->comps[1].data = d1;
    opj_image_data_free(img->comps[2].data);
    img->comps[2].data = d2;

    img->comps[1].w = img->comps[2].w = img->comps[0].w;
    img->comps[1].h = img->comps[2].h = img->comps[0].h;
    img->comps[1].dx = img->comps[2].dx = img->comps[0].dx;
    img->comps[1].dy = img->comps[2].dy = img->comps[0].dy;
    img->color_space = OPJ_CLRSPC_SRGB;
    return;

fails:
    opj_image_data_free(r);
    opj_image_data_free(g);
    opj_image_data_free(b);
} /* sycc420_to_rgb() */

void color_sycc_to_rgb(opj_image_t * img)
{
    if (img->numcomps < 3) {
        img->color_space = OPJ_CLRSPC_GRAY;
        return;
    }

    if ((img->comps[0].dx == 1) && (img->comps[1].dx == 2) && (img->comps[2].dx == 2) && (img->comps[0].dy == 1) &&
        (img->comps[1].dy == 2) && (img->comps[2].dy == 2)) { /* horizontal and vertical sub-sample */
        sycc420_to_rgb(img);
    } else if ((img->comps[0].dx == 1) && (img->comps[1].dx == 2) && (img->comps[2].dx == 2) && (img->comps[0].dy == 1) &&
               (img->comps[1].dy == 1) && (img->comps[2].dy == 1)) { /* horizontal sub-sample only */
        sycc422_to_rgb(img);
    } else if ((img->comps[0].dx == 1) && (img->comps[1].dx == 1) && (img->comps[2].dx == 1) && (img->comps[0].dy == 1) &&
               (img->comps[1].dy == 1) && (img->comps[2].dy == 1)) { /* no sub-sample */
        sycc444_to_rgb(img);
    } else {
        fprintf(stderr, "%s:%d:color_sycc_to_rgb\n\tCAN NOT CONVERT\n", __FILE__, __LINE__);
        return;
    }
} /* color_sycc_to_rgb() */

void color_cmyk_to_rgb(opj_image_t * image)
{
    float C, M, Y, K;
    float sC, sM, sY, sK;
    unsigned int w, h, max, i;

    w = image->comps[0].w;
    h = image->comps[0].h;

    if ((image->numcomps < 4) || (image->comps[0].dx != image->comps[1].dx) || (image->comps[0].dx != image->comps[2].dx) ||
        (image->comps[0].dx != image->comps[3].dx) || (image->comps[0].dy != image->comps[1].dy) ||
        (image->comps[0].dy != image->comps[2].dy) || (image->comps[0].dy != image->comps[3].dy)) {
        fprintf(stderr, "%s:%d:color_cmyk_to_rgb\n\tCAN NOT CONVERT\n", __FILE__, __LINE__);
        return;
    }

    max = w * h;

    sC = 1.0F / (float)((1 << image->comps[0].prec) - 1);
    sM = 1.0F / (float)((1 << image->comps[1].prec) - 1);
    sY = 1.0F / (float)((1 << image->comps[2].prec) - 1);
    sK = 1.0F / (float)((1 << image->comps[3].prec) - 1);

    for (i = 0; i < max; ++i) {
        /* CMYK values from 0 to 1 */
        C = (float)(image->comps[0].data[i]) * sC;
        M = (float)(image->comps[1].data[i]) * sM;
        Y = (float)(image->comps[2].data[i]) * sY;
        K = (float)(image->comps[3].data[i]) * sK;

        /* Invert all CMYK values */
        C = 1.0F - C;
        M = 1.0F - M;
        Y = 1.0F - Y;
        K = 1.0F - K;

        /* CMYK -> RGB : RGB results from 0 to 255 */
        image->comps[0].data[i] = (int)(255.0F * C * K); /* R */
        image->comps[1].data[i] = (int)(255.0F * M * K); /* G */
        image->comps[2].data[i] = (int)(255.0F * Y * K); /* B */
    }

    opj_image_data_free(image->comps[3].data);
    image->comps[3].data = NULL;
    image->comps[0].prec = 8;
    image->comps[1].prec = 8;
    image->comps[2].prec = 8;
    image->numcomps -= 1;
    image->color_space = OPJ_CLRSPC_SRGB;

    for (i = 3; i < image->numcomps; ++i) {
        memcpy(&(image->comps[i]), &(image->comps[i + 1]), sizeof(image->comps[i]));
    }

} /* color_cmyk_to_rgb() */

/*
 * This code has been adopted from sjpx_openjpeg.c of ghostscript
 */
void color_esycc_to_rgb(opj_image_t * image)
{
    int y, cb, cr, sign1, sign2, val;
    unsigned int w, h, max, i;
    int flip_value = (1 << (image->comps[0].prec - 1));
    int max_value = (1 << image->comps[0].prec) - 1;

    if ((image->numcomps < 3) || (image->comps[0].dx != image->comps[1].dx) || (image->comps[0].dx != image->comps[2].dx) ||
        (image->comps[0].dy != image->comps[1].dy) || (image->comps[0].dy != image->comps[2].dy)) {
        fprintf(stderr, "%s:%d:color_esycc_to_rgb\n\tCAN NOT CONVERT\n", __FILE__, __LINE__);
        return;
    }

    w = image->comps[0].w;
    h = image->comps[0].h;

    sign1 = (int)image->comps[1].sgnd;
    sign2 = (int)image->comps[2].sgnd;

    max = w * h;

    for (i = 0; i < max; ++i) {
        y = image->comps[0].data[i];
        cb = image->comps[1].data[i];
        cr = image->comps[2].data[i];

        if (!sign1) {
            cb -= flip_value;
        }
        if (!sign2) {
            cr -= flip_value;
        }

        val = (int)((float)y - (float)0.0000368 * (float)cb + (float)1.40199 * (float)cr + (float)0.5);

        if (val > max_value) {
            val = max_value;
        } else if (val < 0) {
            val = 0;
        }
        image->comps[0].data[i] = val;

        val = (int)((float)1.0003 * (float)y - (float)0.344125 * (float)cb - (float)0.7141128 * (float)cr + (float)0.5);

        if (val > max_value) {
            val = max_value;
        } else if (val < 0) {
            val = 0;
        }
        image->comps[1].data[i] = val;

        val = (int)((float)0.999823 * (float)y + (float)1.77204 * (float)cb - (float)0.000008 * (float)cr + (float)0.5);

        if (val > max_value) {
            val = max_value;
        } else if (val < 0) {
            val = 0;
        }
        image->comps[2].data[i] = val;
    }
    image->color_space = OPJ_CLRSPC_SRGB;

} /* color_esycc_to_rgb() */
