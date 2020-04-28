// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2020.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include <string.h>

#include "JXRGlue.h"

// Y, U, V, YHP, UHP, VHP
int DPK_QPS_420[11][6] = { // for 8 bit only
    { 66, 65, 70, 72, 72, 77 }, { 59, 58, 63, 64, 63, 68 }, { 52, 51, 57, 56, 56, 61 }, { 48, 48, 54, 51, 50, 55 },
    { 43, 44, 48, 46, 46, 49 }, { 37, 37, 42, 38, 38, 43 }, { 26, 28, 31, 27, 28, 31 }, { 16, 17, 22, 16, 17, 21 },
    { 10, 11, 13, 10, 10, 13 }, { 5, 5, 6, 5, 5, 6 },       { 2, 2, 3, 2, 2, 2 }
};

int DPK_QPS_8[12][6] = { { 67, 79, 86, 72, 90, 98 }, { 59, 74, 80, 64, 83, 89 }, { 53, 68, 75, 57, 76, 83 },
                         { 49, 64, 71, 53, 70, 77 }, { 45, 60, 67, 48, 67, 74 }, { 40, 56, 62, 42, 59, 66 },
                         { 33, 49, 55, 35, 51, 58 }, { 27, 44, 49, 28, 45, 50 }, { 20, 36, 42, 20, 38, 44 },
                         { 13, 27, 34, 13, 28, 34 }, { 7, 17, 21, 8, 17, 21 }, // Photoshop 100%
                         { 2, 5, 6, 2, 5, 6 } };

int DPK_QPS_16[11][6] = { { 197, 203, 210, 202, 207, 213 },
                          { 174, 188, 193, 180, 189, 196 },
                          { 152, 167, 173, 156, 169, 174 },
                          { 135, 152, 157, 137, 153, 158 },
                          { 119, 137, 141, 119, 138, 142 },
                          { 102, 120, 125, 100, 120, 124 },
                          { 82, 98, 104, 79, 98, 103 },
                          { 60, 76, 81, 58, 76, 81 },
                          { 39, 52, 58, 36, 52, 58 },
                          { 16, 27, 33, 14, 27, 33 },
                          { 5, 8, 9, 4, 7, 8 } };

struct clImage * clFormatReadJXR(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteJXR(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadJXR(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(overrideProfile);
    COLORIST_UNUSED(input);

    clRaw rawProfile = CL_RAW_EMPTY;
    clProfile * profile = NULL;
    clImage * image = NULL;
    ERR err = WMP_errSuccess;
    uint32_t profileByteCount = 0;

    PKFactory * pFactory = NULL;
    PKCodecFactory * pCodecFactory = NULL;
    PKImageDecode * pDecoder = NULL;
    PKFormatConverter * pConverter = NULL;

    int depth;
    PKPixelInfo pixelFormat;
    PKPixelFormatGUID guidPixFormat;
    U32 frameCount = 0;
    PKRect rect = { 0, 0, 0, 0 };

    memset(&rawProfile, 0, sizeof(rawProfile));

    if (Failed(err = PKCreateFactory(&pFactory, PK_SDK_VERSION))) {
        clContextLogError(C, "Can't create JXR PK factory");
        goto readCleanup;
    }

    if (Failed(err = PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION))) {
        clContextLogError(C, "Can't create JXR codec factory");
        goto readCleanup;
    }

    if (Failed(err = pCodecFactory->CreateDecoderFromMemory(".jxr", input->ptr, input->size, &pDecoder))) {
        clContextLogError(C, "Can't create JXR codec factory");
        goto readCleanup;
    }

    if ((pDecoder->uWidth < 1) || (pDecoder->uHeight < 1)) {
        clContextLogError(C, "Invalid JXR image size (%ux%u)", pDecoder->uWidth, pDecoder->uHeight);
        goto readCleanup;
    }

    pixelFormat.pGUIDPixFmt = &pDecoder->guidPixFormat;
    if (Failed(err = PixelFormatLookup(&pixelFormat, LOOKUP_FORWARD))) {
        clContextLogError(C, "Unrecognized pixel format (1)");
        goto readCleanup;
    }
    if (Failed(err = PixelFormatLookup(&pixelFormat, LOOKUP_BACKWARD_TIF))) {
        clContextLogError(C, "Unrecognized pixel format (2)");
        goto readCleanup;
    }

    pDecoder->GetColorContext(pDecoder, NULL, &profileByteCount);
    if (profileByteCount > 0) {
        clRawRealloc(C, &rawProfile, profileByteCount);
        if (Failed(err = pDecoder->GetColorContext(pDecoder, rawProfile.ptr, &profileByteCount))) {
            clContextLogError(C, "Can't read JXR format's ICC profile");
            goto readCleanup;
        }
        profile = clProfileParse(C, rawProfile.ptr, rawProfile.size, NULL);
        if (!profile) {
            clContextLogError(C, "Invalid ICC profile in JXR");
            goto readCleanup;
        }
    } else {
        // crazy hacks

        clProfilePrimaries primaries;
        primaries.red[0] = 0.64f;
        primaries.red[1] = 0.33f;
        primaries.green[0] = 0.30f;
        primaries.green[1] = 0.60f;
        primaries.blue[0] = 0.15f;
        primaries.blue[1] = 0.06f;
        primaries.white[0] = 0.3127f;
        primaries.white[1] = 0.3290f;

        clProfileCurve curve;
        curve.type = CL_PCT_GAMMA;
        curve.gamma = 2.2f;

        profile = clProfileCreate(C, &primaries, &curve, 80, NULL);
    }

    // if (pixelFormat.uBitsPerSample > 8) {
    //     depth = 16;
    //     guidPixFormat = GUID_PKPixelFormat64bppRGBA;
    // } else {
    //     depth = 8;
    //     guidPixFormat = GUID_PKPixelFormat32bppRGBA;
    // }

    depth = 32;
    guidPixFormat = GUID_PKPixelFormat128bppRGBAFloat;

    if (Failed(err = pDecoder->GetFrameCount(pDecoder, &frameCount)) || (frameCount < 1)) {
        clContextLogError(C, "Invalid JXR frame count (%d)", frameCount);
        goto readCleanup;
    }

    if (Failed(err = pCodecFactory->CreateFormatConverter(&pConverter))) {
        clContextLogError(C, "Can't create JXR format converter");
        goto readCleanup;
    }

    if (Failed(err = pConverter->Initialize(pConverter, pDecoder, "jxr", guidPixFormat))) {
        clContextLogError(C, "Can't initialize JXR format converter");
        goto readCleanup;
    }

    rect.Width = pDecoder->uWidth;
    rect.Height = pDecoder->uHeight;
    image = clImageCreate(C, pDecoder->uWidth, pDecoder->uHeight, depth, profile);
    clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_F32);
    if (Failed(err = pConverter->Copy(pConverter, &rect, (U8 *)image->pixelsF32, image->width * 4 * sizeof(float)))) {
        clContextLogError(C, "Can't copy JXR pixels");
        clImageDestroy(C, image);
        image = NULL;
        goto readCleanup;
    }

readCleanup:
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;
}

clBool clFormatWriteJXR(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(image);
    COLORIST_UNUSED(formatName);
    COLORIST_UNUSED(output);
    COLORIST_UNUSED(writeParams);

    clBool writeResult = clTrue;
#if 0
    WebPConfig config;
    WebPPicture picture;
    WebPMemoryWriter memoryWriter;

    WebPData imageChunk, assembledChunk;
    WebPMux * mux = NULL;

    memset(&imageChunk, 0, sizeof(imageChunk));
    memset(&assembledChunk, 0, sizeof(assembledChunk));

    WebPMemoryWriterInit(&memoryWriter);
    WebPConfigInit(&config);
    WebPPictureInit(&picture);

    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        goto writeCleanup;
    }

    config.lossless = (writeParams->quality >= 100) ? 1 : 0;
    config.emulate_jpeg_size = 1; // consistency across export quality values
    config.quality = (float)writeParams->quality;
    config.method = 6; // always go for the best output, encoding speed be damned

    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = (void *)&memoryWriter;
    picture.use_argb = 1;
    picture.width = image->width;
    picture.height = image->height;

    clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_U8);
    WebPPictureImportRGBA(&picture, image->pixelsU8, CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_U8) * image->width);

    if (!WebPEncode(&config, &picture)) {
        clContextLogError(C, "Failed to encode WebP");
        writeResult = clFalse;
        goto writeCleanup;
    }

    mux = WebPMuxNew();

    if (writeParams->writeProfile) {
        WebPData iccChunk;
        memset(&iccChunk, 0, sizeof(iccChunk));
        iccChunk.bytes = rawProfile.ptr;
        iccChunk.size = rawProfile.size;
        if (!WebPMuxSetChunk(mux, "ICCP", &iccChunk, 0)) {
            clContextLogError(C, "Failed create ICC profile");
            goto writeCleanup;
        }
    }

    imageChunk.bytes = memoryWriter.mem;
    imageChunk.size = memoryWriter.size;
    WebPMuxSetImage(mux, &imageChunk, 0);
    if (WebPMuxAssemble(mux, &assembledChunk) != WEBP_MUX_OK) {
        clContextLogError(C, "Failed to assemble WebP");
        goto writeCleanup;
    }

    clRawSet(C, output, assembledChunk.bytes, assembledChunk.size);

writeCleanup:
    if (mux) {
        WebPMuxDelete(mux);
    }
    WebPDataClear(&assembledChunk);
    WebPMemoryWriterClear(&memoryWriter);
    WebPPictureFree(&picture);
    clRawFree(C, &rawProfile);
#endif
    return writeResult;
}
