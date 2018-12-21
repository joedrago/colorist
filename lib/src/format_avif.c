// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/yuv.h"

#include <string.h>
#include <stdlib.h>

#include "aom/aom_decoder.h"
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "aom/aomdx.h"

struct clImage * clFormatReadAVIF(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteAVIF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadAVIF(struct clContext * C, const char * formatName, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(input);

    clImage * image = NULL;
    clProfile * profile = NULL;
    clYUV * yuv = NULL;

    clBool initializedDecoder = clFalse;
    aom_codec_ctx_t decoder;
    aom_codec_stream_info_t si;
    aom_codec_iface_t * decoder_interface = aom_codec_av1_dx();

    if (aom_codec_dec_init(&decoder, decoder_interface, NULL, 0)) {
        clContextLogError(C, "Failed to initialize AV1 decoder");
        goto readCleanup;
    }
    initializedDecoder = clTrue;

    if (aom_codec_control(&decoder, AV1D_SET_OUTPUT_ALL_LAYERS, 1)) {
        clContextLogError(C, "Failed to set AV1 output_all_layers control");
        goto readCleanup;
    }

    if (input->size < 32) {
        clContextLogError(C, "Too small to be an AV1");
        goto readCleanup;
    }

    si.is_annexb = 0;
    if (aom_codec_peek_stream_info(decoder_interface, input->ptr, input->size, &si)) {
        clContextLogError(C, "Not a valid AV1 OBU");
        goto readCleanup;
    }

    if (aom_codec_decode(&decoder, input->ptr, input->size, NULL)) {
        clContextLogError(C, "Failed to decode AV1 OBU");
        goto readCleanup;
    }

    aom_codec_iter_t iter = NULL;
    aom_image_t * aomImage = aom_codec_get_frame(&decoder, &iter); // It doesn't appear that I own this / need to free this
    if (!aomImage) {
        clContextLogError(C, "Couldn't find an AVI frame");
        goto readCleanup;
    }

    image = clImageCreate(C, aomImage->d_w, aomImage->d_h, 16, NULL);

    yuv = clYUVCreate(C, image->profile);
    float yuvPixel[3];
    float rgbPixel[3];
    uint16_t * dstPixels = (uint16_t *)image->pixels;
    for (int j = 0; j < image->height; ++j) {
        for (int i = 0; i < image->width; ++i) {
            for (int plane = 0; plane < 3; ++plane) {
                uint16_t * planePixel = (uint16_t *)&aomImage->planes[plane][(j * aomImage->stride[plane]) + (2 * i)];
                yuvPixel[plane] = *planePixel / 4095.0f;
            }
            yuvPixel[1] -= 0.5f;
            yuvPixel[2] -= 0.5f;
            uint16_t * dstPixel = &dstPixels[4 * (i + (j * image->width))];
            clYUVConvertYUV444toRGB(C, yuv, yuvPixel, rgbPixel);
            dstPixel[0] = (uint16_t)(rgbPixel[0] * 65535.0f);
            dstPixel[1] = (uint16_t)(rgbPixel[1] * 65535.0f);
            dstPixel[2] = (uint16_t)(rgbPixel[2] * 65535.0f);
            dstPixel[3] = 65535; // TODO: alpha
        }
    }

readCleanup:
    if (initializedDecoder) {
        aom_codec_destroy(&decoder);
    }
    if (profile) {
        clProfileDestroy(C, profile);
    }
    if (yuv) {
        clYUVDestroy(C, yuv);
    }
    return image;
}

clBool clFormatWriteAVIF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);

    COLORIST_UNUSED(image);
    COLORIST_UNUSED(writeParams);

    clBool writeResult = clFalse;

    aom_codec_iface_t * encoder_interface = aom_codec_av1_cx();

    clYUV * yuv = clYUVCreate(C, image->profile);

    aom_image_t * aomImage = aom_img_alloc(NULL, AOM_IMG_FMT_I44416, image->width, image->height, 16);
    aomImage->range = AOM_CR_FULL_RANGE; // always use full range

    float yuvPixel[3];
    float rgbPixel[3];
    if (image->depth == 16) {
        uint16_t * srcPixels = (uint16_t *)image->pixels;
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint16_t * srcPixel = &srcPixels[4 * (i + (j * image->width))];
                rgbPixel[0] = srcPixel[0] / 65535.0f;
                rgbPixel[1] = srcPixel[1] / 65535.0f;
                rgbPixel[2] = srcPixel[2] / 65535.0f;
                clYUVConvertRGBtoYUV444(C, yuv, rgbPixel, yuvPixel);
                yuvPixel[0] = CL_CLAMP(yuvPixel[0], 0.0f, 1.0f);
                yuvPixel[1] += 0.5f;
                yuvPixel[1] = CL_CLAMP(yuvPixel[1], 0.0f, 1.0f);
                yuvPixel[2] += 0.5f;
                yuvPixel[2] = CL_CLAMP(yuvPixel[2], 0.0f, 1.0f);
                for (int plane = 0; plane < 3; ++plane) {
                    uint16_t * planePixel = (uint16_t *)&aomImage->planes[plane][(j * aomImage->stride[plane]) + (2 * i)];
                    *planePixel = (uint16_t)(yuvPixel[plane] * 4095.0f);
                }
            }
        }
    } else {
        uint8_t * srcPixels = image->pixels;
        for (int j = 0; j < image->height; ++j) {
            for (int i = 0; i < image->width; ++i) {
                uint8_t * srcPixel = &srcPixels[4 * (i + (j * image->width))];
                rgbPixel[0] = srcPixel[0] / 255.0f;
                rgbPixel[1] = srcPixel[1] / 255.0f;
                rgbPixel[2] = srcPixel[2] / 255.0f;
                clYUVConvertRGBtoYUV444(C, yuv, rgbPixel, yuvPixel);
                yuvPixel[0] = CL_CLAMP(yuvPixel[0], 0.0f, 1.0f);
                yuvPixel[1] += 0.5f;
                yuvPixel[1] = CL_CLAMP(yuvPixel[1], 0.0f, 1.0f);
                yuvPixel[2] += 0.5f;
                yuvPixel[2] = CL_CLAMP(yuvPixel[2], 0.0f, 1.0f);
                for (int plane = 0; plane < 3; ++plane) {
                    uint16_t * planePixel = (uint16_t *)&aomImage->planes[plane][(j * aomImage->stride[plane]) + (2 * i)];
                    *planePixel = (uint16_t)(yuvPixel[plane] * 4095.0f);
                }
            }
        }
    }

    struct aom_codec_enc_cfg cfg;
    aom_codec_enc_config_default(encoder_interface, &cfg, 0);

    // Profile 2.  8-bit and 10-bit 4:2:2
    //            12-bit  4:0:0, 4:2:2 and 4:4:4
    cfg.g_profile = 2;
    cfg.g_bit_depth = AOM_BITS_12;
    cfg.g_input_bit_depth = 12;
    cfg.g_w = image->width;
    cfg.g_h = image->height;
    cfg.g_threads = C->params.jobs;

    aom_codec_ctx_t encoder;
    aom_codec_enc_init(&encoder, encoder_interface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH);

    aom_codec_control(&encoder, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE);

    // aom_codec_control(&encoder, AV1E_SET_ROW_MT, 1);
    // aom_codec_control(&encoder, AV1E_SET_TILE_ROWS, 8);
    // aom_codec_control(&encoder, AV1E_SET_TILE_COLUMNS, 8);

    aom_codec_encode(&encoder, aomImage, 0, 1, 0);
    aom_codec_encode(&encoder, NULL, 0, 1, 0); // flush

    const aom_codec_cx_pkt_t * pkt;
    aom_codec_iter_t iter = NULL;
    for (;;) {
        pkt = aom_codec_get_cx_data(&encoder, &iter);
        if (pkt == NULL)
            break;
        if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
            clRawSet(C, output, pkt->data.frame.buf, pkt->data.frame.sz);
            writeResult = clTrue;
            break;
        }
    }

    clYUVDestroy(C, yuv);
    return writeResult;
}
