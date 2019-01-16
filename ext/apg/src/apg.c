// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2019.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "apg.h"

#include "aom/aom_decoder.h"
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "aom/aomdx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Constants

#define APG_MAGIC "APG!"

#define APG_SRGB_YUV_COEFF_R 13936 // round(~0.2126 * 65535)
#define APG_SRGB_YUV_COEFF_G 46868 // round(~0.7152 * 65535)
#define APG_SRGB_YUV_COEFF_B 4733  // round(~0.0722 * 65535)

#define APG_REASONABLE_DIMENSION 32768               // arbitrarily large
#define APG_REASONABLE_ICC_PROFILE_SIZE (256 * 1024) // This is pretty generous/ridiculous

// ---------------------------------------------------------------------------
// Macros and Forwards

// Yes, clamp macros are nasty. Do not use them.
#define APG_CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static uint16_t apgHTONS(uint16_t s);
static uint16_t apgNTOHS(uint16_t s);
static uint32_t apgHTONL(uint32_t l);
static uint32_t apgNTOHL(uint32_t l);

// ---------------------------------------------------------------------------
// Internal structures

typedef struct apgLayer
{
    aom_codec_ctx_t encoder;
    aom_codec_ctx_t decoder;
    apgBool codecInitialized;

    aom_image_t * image;
    uint8_t * obu;
    uint32_t obuSize;
} apgLayer;

typedef enum apgLayerType
{
    LT_COLOR = 0,
    LT_ALPHA,

    LT_COUNT
} apgLayerType;

// ---------------------------------------------------------------------------
// apgImage

apgImage * apgImageCreate(int width, int height, int depth)
{
    apgImage * image = calloc(1, sizeof(apgImage));
    image->width = width;
    image->height = height;
    image->depth = depth;
    image->pixels = calloc(4 * sizeof(uint16_t), width * height);
    image->yuvKR = APG_SRGB_YUV_COEFF_R;
    image->yuvKG = APG_SRGB_YUV_COEFF_G;
    image->yuvKB = APG_SRGB_YUV_COEFF_B;
    return image;
}

void apgImageDestroy(apgImage * image)
{
    apgImageSetICC(image, NULL, 0);
    free(image->pixels);
    free(image);
}

void apgImageSetICC(apgImage * image, uint8_t * icc, uint32_t iccSize)
{
    if (image->icc) {
        free(image->icc);
        image->icc = NULL;
    }
    image->iccSize = 0;

    if (icc && iccSize) {
        image->iccSize = iccSize;
        image->icc = calloc(1, iccSize);
        memcpy(image->icc, icc, iccSize);
    }
}

void apgImageSetYUVCoefficients(apgImage * image, float yuvKR, float yuvKG, float yuvKB)
{
    image->yuvKR = (uint16_t)(floorf((65535.0f * yuvKR) + 0.5f));
    image->yuvKG = (uint16_t)(floorf((65535.0f * yuvKG) + 0.5f));
    image->yuvKB = (uint16_t)(floorf((65535.0f * yuvKB) + 0.5f));
}

void apgImageSetYUVCoefficientsFromPrimaries(apgImage * image, float xr, float yr, float xg, float yg, float xb, float yb, float xw, float yw)
{
    // TODO: implement
}

// ---------------------------------------------------------------------------
// Encode

apgResult apgImageEncode(apgImage * image, int quality)
{
    apgResult result = APG_RESULT_UNKNOWN_ERROR;
    aom_codec_iface_t * encoder_interface = aom_codec_av1_cx();

    // Cleanup any lingering data
    if (image->encoded) {
        free(image->encoded);
        image->encoded = NULL;
    }
    image->encodedSize = 0;

    // Init all layers
    apgLayer layers[LT_COUNT];
    memset(layers, 0, sizeof(layers));
    for (int layerType = 0; layerType < LT_COUNT; ++layerType) {
        apgLayer * layer = &layers[layerType];

        layer->image = aom_img_alloc(NULL, AOM_IMG_FMT_I44416, image->width, image->height, 16);
        layer->image->range = AOM_CR_FULL_RANGE; // always use full range
        if (layerType == LT_ALPHA) {
            layer->image->monochrome = 1;
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
        // cfg.g_threads = ...;

        apgBool lossless = (quality == 0) || (quality == 100) || (layerType == LT_ALPHA); // alpha is always lossless
        if (lossless) {
            cfg.rc_min_quantizer = 0;
            cfg.rc_max_quantizer = 0;
        } else {
            int rescaledQuality = 63 - (int)(((float)quality / 100.0f) * 63.0f);
            cfg.rc_min_quantizer = 0;
            cfg.rc_max_quantizer = rescaledQuality;
        }

        aom_codec_enc_init(&layer->encoder, encoder_interface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH);
        aom_codec_control(&layer->encoder, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE);
        if (lossless) {
            aom_codec_control(&layer->encoder, AV1E_SET_LOSSLESS, 1);
        }
    }

    // Populate aom_image_t pixel data
    float kr = (float)image->yuvKR / 65535.0f;
    float kg = (float)image->yuvKG / 65535.0f;
    float kb = (float)image->yuvKB / 65535.0f;
    float yuvPixel[3];
    float rgbPixel[3];
    float maxChannel = (float)((1 << image->depth) - 1);
    uint16_t * srcPixels = (uint16_t *)image->pixels;
    for (int j = 0; j < image->height; ++j) {
        for (int i = 0; i < image->width; ++i) {
            // Unpack RGB into normalized float
            uint16_t * srcPixel = &srcPixels[4 * (i + (j * image->width))];
            rgbPixel[0] = srcPixel[0] / maxChannel;
            rgbPixel[1] = srcPixel[1] / maxChannel;
            rgbPixel[2] = srcPixel[2] / maxChannel;

            // RGB -> YUV conversion
            float Y = (kr * rgbPixel[0]) + (kg * rgbPixel[1]) + (kb * rgbPixel[2]);
            yuvPixel[0] = Y;
            yuvPixel[1] = (rgbPixel[2] - Y) / (2 * (1 - kb));
            yuvPixel[2] = (rgbPixel[0] - Y) / (2 * (1 - kr));

            // Stuff YUV into unorm16 color layer
            yuvPixel[0] = APG_CLAMP(yuvPixel[0], 0.0f, 1.0f);
            yuvPixel[1] += 0.5f;
            yuvPixel[1] = APG_CLAMP(yuvPixel[1], 0.0f, 1.0f);
            yuvPixel[2] += 0.5f;
            yuvPixel[2] = APG_CLAMP(yuvPixel[2], 0.0f, 1.0f);
            for (int plane = 0; plane < 3; ++plane) {
                uint16_t * planePixel = (uint16_t *)&layers[LT_COLOR].image->planes[plane][(j * layers[LT_COLOR].image->stride[plane]) + (2 * i)];
                *planePixel = (uint16_t)(yuvPixel[plane] * 4095.0f);
            }

            // Stuff alpha into unorm16 alpha layer
            uint16_t * alphaPixel = (uint16_t *)&layers[LT_ALPHA].image->planes[0][(j * layers[LT_ALPHA].image->stride[0]) + (2 * i)];
            *alphaPixel = (uint16_t)((srcPixel[3] / maxChannel) * 4095.0f);
        }
    }

    // Encode layers
    apgBool encodedAllLayers = apgTrue;
    for (int layerType = 0; layerType < LT_COUNT; ++layerType) {
        apgLayer * layer = &layers[layerType];

        aom_codec_encode(&layer->encoder, layer->image, 0, 1, 0);
        aom_codec_encode(&layer->encoder, NULL, 0, 1, 0); // flush

        aom_codec_iter_t iter = NULL;
        for (;;) {
            const aom_codec_cx_pkt_t * pkt = aom_codec_get_cx_data(&layer->encoder, &iter);
            if (pkt == NULL)
                break;
            if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
                layer->obu = pkt->data.frame.buf;
                layer->obuSize = (uint32_t)pkt->data.frame.sz;
                break;
            }
        }

        if (!layer->obu || !layer->obuSize) {
            encodedAllLayers = apgFalse;
        }
    }

    // Build payload
    if (encodedAllLayers) {
        uint32_t payloadSize = APG_HEADER_SIZE_V1 + image->iccSize + layers[LT_COLOR].obuSize + layers[LT_ALPHA].obuSize;
        uint8_t * payload = calloc(1, payloadSize);

        // Offset   Size     Type      Description           Notes
        // ------   ----     ----      --------------------- -------------------------

        //      0      4   fourcc      Magic: APG!
        memcpy(payload + 0, APG_MAGIC, 4);

        //      4      4      u32      Version               (network order, always 1)
        uint32_t versionNO = apgHTONL(1);
        memcpy(payload + 4, &versionNO, sizeof(versionNO));

        //      8      4      u32      Width                 (network order)
        uint32_t widthNO = apgHTONL(image->width);
        memcpy(payload + 8, &widthNO, sizeof(widthNO));

        //     12      4      u32      Height                (network order)
        uint32_t heightNO = apgHTONL(image->height);
        memcpy(payload + 12, &heightNO, sizeof(heightNO));

        //     16      2      u16      Depth                 (network order)
        uint16_t depthNO = apgHTONS(image->depth);
        memcpy(payload + 16, &depthNO, sizeof(depthNO));

        //     18      2      u16      YUV Coeff: Red        (network order, 65535x)
        uint16_t krNO = apgHTONS(image->yuvKR);
        memcpy(payload + 18, &krNO, sizeof(krNO));

        //     20      2      u16      YUV Coeff: Green      (network order, 65535x)
        uint16_t kgNO = apgHTONS(image->yuvKG);
        memcpy(payload + 20, &kgNO, sizeof(kgNO));

        //     22      2      u16      YUV Coeff: Blue       (network order, 65535x)
        uint16_t kbNO = apgHTONS(image->yuvKB);
        memcpy(payload + 22, &kbNO, sizeof(kbNO));

        //     24      4      u32      ICC Payload Size      (network order)
        uint32_t iccSizeNO = apgHTONL(image->iccSize);
        memcpy(payload + 24, &iccSizeNO, sizeof(iccSizeNO));

        //     28      4      u32      Color Payload Size    (network order)
        uint32_t colorOBUSizeNO = apgHTONL(layers[LT_COLOR].obuSize);
        memcpy(payload + 28, &colorOBUSizeNO, sizeof(colorOBUSizeNO));

        //     32      4      u32      Alpha Payload Size    (network order)
        uint32_t alphaOBUSizeNO = apgHTONL(layers[LT_ALPHA].obuSize);
        memcpy(payload + 32, &alphaOBUSizeNO, sizeof(alphaOBUSizeNO));

        //     36      x    bytes      ICC Payload           (may be 0 bytes)
        if ((image->iccSize > 0) && image->icc) {
            memcpy(payload + 36, image->icc, image->iccSize);
        }

        //   36+x      y    bytes      Color Payload         (AV1 OBUs)
        memcpy(payload + 36 + image->iccSize, layers[LT_COLOR].obu, layers[LT_COLOR].obuSize);

        // 36+x+y      z    bytes      Alpha Payload         (AV1 OBUs)
        memcpy(payload + 36 + image->iccSize + layers[LT_COLOR].obuSize, layers[LT_ALPHA].obu, layers[LT_ALPHA].obuSize);

        image->encoded = payload;
        image->encodedSize = payloadSize;
        result = APG_RESULT_OK;
    }

    // Cleanup
    for (int layerType = 0; layerType < LT_COUNT; ++layerType) {
        apgLayer * layer = &layers[layerType];

        aom_img_free(layer->image);
        aom_codec_destroy(&layer->encoder);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Decode

apgImage * apgImageDecode(uint8_t * encoded, uint32_t encodedSize, apgResult * result)
{
    if (encodedSize < APG_HEADER_SIZE_V1) {
        *result = APG_RESULT_TRUNCATED;
        return NULL;
    }

    uint32_t leftoverSize = encodedSize - APG_HEADER_SIZE_V1;

    *result = APG_RESULT_UNKNOWN_ERROR;

    apgLayer layers[LT_COUNT];
    memset(layers, 0, sizeof(layers));

    // Offset   Size     Type      Description           Notes
    // ------   ----     ----      --------------------- -------------------------

    //      0      4   fourcc      Magic: APG!
    uint8_t magic[4];
    memcpy(magic, encoded + 0, 4);
    if (memcmp(magic, APG_MAGIC, 4) != 0) {
        *result = APG_RESULT_INVALID_HEADER;
        return NULL;
    }

    //      4      4      u32      Version               (network order, always 1)
    uint32_t version;
    memcpy(&version, encoded + 4, sizeof(version));
    version = apgNTOHL(version);
    if (version != 1) {
        *result = APG_RESULT_UNSUPPORTED_VERSION;
        return NULL;
    }

    //      8      4      u32      Width                 (network order)
    uint32_t width;
    memcpy(&width, encoded + 8, sizeof(width));
    width = apgNTOHL(width);
    if (width > APG_REASONABLE_DIMENSION) {
        *result = APG_RESULT_INVALID_HEADER;
        return NULL;
    }

    //     12      4      u32      Height                (network order)
    uint32_t height;
    memcpy(&height, encoded + 12, sizeof(height));
    height = apgNTOHL(height);
    if (height > APG_REASONABLE_DIMENSION) {
        *result = APG_RESULT_INVALID_HEADER;
        return NULL;
    }

    //     16      2      u16      Depth                 (network order)
    uint16_t depth;
    memcpy(&depth, encoded + 16, sizeof(depth));
    depth = apgNTOHS(depth);
    if ((depth < 8) || (depth > 16)) {
        *result = APG_RESULT_INVALID_HEADER;
        return NULL;
    }

    //     18      2      u16      YUV Coeff: Red        (network order, 65535x)
    uint16_t rawKR;
    memcpy(&rawKR, encoded + 18, sizeof(rawKR));
    rawKR = apgNTOHS(rawKR);

    //     20      2      u16      YUV Coeff: Green      (network order, 65535x)
    uint16_t rawKG;
    memcpy(&rawKG, encoded + 20, sizeof(rawKG));
    rawKG = apgNTOHS(rawKG);

    //     22      2      u16      YUV Coeff: Blue       (network order, 65535x)
    uint16_t rawKB;
    memcpy(&rawKB, encoded + 22, sizeof(rawKB));
    rawKB = apgNTOHS(rawKB);

    //     24      4      u32      ICC Payload Size      (network order)
    uint32_t iccSize;
    memcpy(&iccSize, encoded + 24, sizeof(iccSize));
    iccSize = apgNTOHL(iccSize);
    if (iccSize > APG_REASONABLE_ICC_PROFILE_SIZE) {
        *result = APG_RESULT_INVALID_HEADER;
        return NULL;
    }
    if (leftoverSize < iccSize) {
        *result = APG_RESULT_TRUNCATED;
        return NULL;
    }
    leftoverSize -= iccSize;

    //     28      4      u32      Color Payload Size    (network order)
    uint32_t colorOBUSizeNO;
    memcpy(&colorOBUSizeNO, encoded + 28, sizeof(colorOBUSizeNO));
    layers[LT_COLOR].obuSize = apgNTOHL(colorOBUSizeNO);
    if (leftoverSize < layers[LT_COLOR].obuSize) {
        *result = APG_RESULT_TRUNCATED;
        return NULL;
    }
    leftoverSize -= layers[LT_COLOR].obuSize;

    //     32      4      u32      Alpha Payload Size    (network order)
    uint32_t alphaOBUSizeNO;
    memcpy(&alphaOBUSizeNO, encoded + 32, sizeof(alphaOBUSizeNO));
    layers[LT_ALPHA].obuSize = apgNTOHL(alphaOBUSizeNO);
    if (leftoverSize < layers[LT_ALPHA].obuSize) {
        *result = APG_RESULT_TRUNCATED;
        return NULL;
    }
    leftoverSize -= layers[LT_ALPHA].obuSize;

    if (leftoverSize != 0) {
        *result = APG_RESULT_TRAILING_DATA;
        return NULL;
    }

    // Get OBU pointers
    layers[LT_COLOR].obu = encoded + APG_HEADER_SIZE_V1 + iccSize;
    layers[LT_ALPHA].obu = encoded + APG_HEADER_SIZE_V1 + iccSize + layers[LT_COLOR].obuSize;

    // Decode OBUs
    for (int layerType = 0; layerType < LT_COUNT; ++layerType) {
        apgLayer * layer = &layers[layerType];
        layer->codecInitialized = apgFalse;

        aom_codec_stream_info_t si;
        aom_codec_iface_t * decoder_interface = aom_codec_av1_dx();
        if (aom_codec_dec_init(&layer->decoder, decoder_interface, NULL, 0)) {
            *result = APG_RESULT_CODEC_INIT_FAILURE;
            return NULL;
        }
        layer->codecInitialized = apgTrue;

        if (aom_codec_control(&layer->decoder, AV1D_SET_OUTPUT_ALL_LAYERS, 1)) {
            *result = APG_RESULT_CODEC_INIT_FAILURE;
            goto decodeCleanup;
        }

        si.is_annexb = 0;
        if (aom_codec_peek_stream_info(decoder_interface, layer->obu, layer->obuSize, &si)) {
            *result = APG_RESULT_INVALID_AV1_PAYLOAD;
            goto decodeCleanup;
        }

        if (aom_codec_decode(&layer->decoder, layer->obu, layer->obuSize, NULL)) {
            *result = APG_RESULT_DECODE_FAILURE;
            goto decodeCleanup;
        }

        aom_codec_iter_t iter = NULL;
        aom_image_t * aomImage = aom_codec_get_frame(&layer->decoder, &iter); // It doesn't appear that I own this / need to free this
        if (!aomImage) {
            *result = APG_RESULT_DECODE_FAILURE;
            goto decodeCleanup;
        }

        if ((aomImage->bit_depth != 12) || (aomImage->fmt != AOM_IMG_FMT_I44416)) {
            *result = APG_RESULT_UNSUPPORTED_FORMAT;
            goto decodeCleanup;
        }

        if ((width != aomImage->d_w) || (height != aomImage->d_h)) {
            *result = APG_RESULT_INCONSISTENT_SIZES;
            goto decodeCleanup;
        }

        layer->image = aomImage;
    }

    apgImage * image = apgImageCreate(width, height, depth);
    image->yuvKR = rawKR;
    image->yuvKG = rawKG;
    image->yuvKB = rawKB;

    if (iccSize > 0) {
        apgImageSetICC(image, encoded + APG_HEADER_SIZE_V1, iccSize);
    }

    float kr = (float)image->yuvKR / 65535.0f;
    float kg = (float)image->yuvKG / 65535.0f;
    float kb = (float)image->yuvKB / 65535.0f;
    float yuvPixel[3];
    float rgbPixel[3];
    float maxChannel = (float)((1 << image->depth) - 1);
    uint16_t * dstPixels = (uint16_t *)image->pixels;
    apgLayer * colorLayer = &layers[LT_COLOR];
    apgLayer * alphaLayer = &layers[LT_ALPHA];
    for (int j = 0; j < image->height; ++j) {
        for (int i = 0; i < image->width; ++i) {
            for (int plane = 0; plane < 3; ++plane) {
                uint16_t * planePixel = (uint16_t *)&colorLayer->image->planes[plane][(j * colorLayer->image->stride[plane]) + (2 * i)];
                yuvPixel[plane] = *planePixel / 4095.0f;
            }
            yuvPixel[1] -= 0.5f;
            yuvPixel[2] -= 0.5f;
            uint16_t * dstPixel = &dstPixels[4 * (i + (j * image->width))];

            float Y  = yuvPixel[0];
            float Cb = yuvPixel[1];
            float Cr = yuvPixel[2];

            float R = Y + (2 * (1 - kr)) * Cr;
            float B = Y + (2 * (1 - kb)) * Cb;
            float G = Y - (
                (2 * ((kr * (1 - kr) * Cr) + (kb * (1 - kb) * Cb)))
                /
                kg);

            rgbPixel[0] = APG_CLAMP(R, 0.0f, 1.0f);
            rgbPixel[1] = APG_CLAMP(G, 0.0f, 1.0f);
            rgbPixel[2] = APG_CLAMP(B, 0.0f, 1.0f);

            uint16_t * alphaPixel = (uint16_t *)&alphaLayer->image->planes[0][(j * alphaLayer->image->stride[0]) + (2 * i)];

            dstPixel[0] = (uint16_t)(rgbPixel[0] * maxChannel);
            dstPixel[1] = (uint16_t)(rgbPixel[1] * maxChannel);
            dstPixel[2] = (uint16_t)(rgbPixel[2] * maxChannel);
            dstPixel[3] = (uint16_t)(((float)alphaPixel[0] / 4095.0f) * maxChannel);
        }
    }

    *result = APG_RESULT_OK;
decodeCleanup:
    for (int layerType = 0; layerType < LT_COUNT; ++layerType) {
        apgLayer * layer = &layers[layerType];

        if (layer->codecInitialized) {
            aom_codec_destroy(&layer->decoder);
        }
    }
    if (image && (*result != APG_RESULT_OK)) {
        apgImageDestroy(image);
        image = NULL;
    }
    return image;
}

// ---------------------------------------------------------------------------
// Helper functions

// Thanks, Rob Pike! https://commandcenter.blogspot.nl/2012/04/byte-order-fallacy.html

static uint16_t apgHTONS(uint16_t s)
{
    uint8_t data[2];
    data[0] = (s >> 8) & 0xff;
    data[1] = (s >> 0) & 0xff;
    uint16_t result;
    memcpy(&result, data, sizeof(uint16_t));
    return result;
}

static uint16_t apgNTOHS(uint16_t s)
{
    uint8_t data[2];
    memcpy(&data, &s, sizeof(data));

    return (uint16_t)((data[1] << 0)
                      | (data[0] << 8));
}

static uint32_t apgHTONL(uint32_t l)
{
    uint8_t data[4];
    data[0] = (l >> 24) & 0xff;
    data[1] = (l >> 16) & 0xff;
    data[2] = (l >> 8) & 0xff;
    data[3] = (l >> 0) & 0xff;
    uint32_t result;
    memcpy(&result, data, sizeof(uint32_t));
    return result;
}

static uint32_t apgNTOHL(uint32_t l)
{
    uint8_t data[4];
    memcpy(&data, &l, sizeof(data));

    return ((uint32_t)data[3] << 0)
           | ((uint32_t)data[2] << 8)
           | ((uint32_t)data[1] << 16)
           | ((uint32_t)data[0] << 24);
}
