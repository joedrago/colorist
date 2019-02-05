// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include "decode.h"
#include "encode.h"
#include "mux.h"

#include <string.h>

struct clImage * clFormatReadWebP(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteWebP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadWebP(struct clContext * C, const char * formatName, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);

    clImage * image = NULL;
    clProfile * profile = NULL;
    uint8_t * readPixels = NULL;

    WebPData webpFileContents;
    webpFileContents.bytes = input->ptr;
    webpFileContents.size = input->size;
    WebPMux * mux = WebPMuxCreate(&webpFileContents, 0);

    uint32_t muxFlags;
    WebPMuxGetFeatures(mux, &muxFlags);

    if (muxFlags & ICCP_FLAG) {
        WebPData iccChunk;
        if (WebPMuxGetChunk(mux, "ICCP", &iccChunk) != WEBP_MUX_OK) {
            clContextLogError(C, "Failed get ICC profile chunk");
            goto readCleanup;
        }
        profile = clProfileParse(C, iccChunk.bytes, iccChunk.size, NULL);
        if (!profile) {
            clContextLogError(C, "Failed parse ICC profile chunk");
            goto readCleanup;
        }
    }

    WebPMuxFrameInfo frameInfo;
    memset(&frameInfo, 0, sizeof(frameInfo));
    if (WebPMuxGetFrame(mux, 1, &frameInfo) != WEBP_MUX_OK) {
        clContextLogError(C, "Failed to get frame chunk in WebP");
        goto readCleanup;
    }

    int width, height;
    readPixels = WebPDecodeRGBA(frameInfo.bitstream.bytes, frameInfo.bitstream.size, &width, &height);
    if (!readPixels) {
        clContextLogError(C, "Failed to decode WebP");
        goto readCleanup;
    }

    clImageLogCreate(C, width, height, 8, profile);
    image = clImageCreate(C, width, height, 8, profile);
    clImageFromRGBA8(C, image, readPixels);

readCleanup:
    WebPDataClear(&frameInfo.bitstream);
    if (readPixels) {
        WebPFree(readPixels);
    }
    if (mux) {
        WebPMuxDelete(mux);
    }
    if (profile) {
        clProfileDestroy(C, profile);
    }
    return image;
}

clBool clFormatWriteWebP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);

    clBool writeResult = clTrue;

    WebPConfig config;
    WebPPicture picture;
    WebPMemoryWriter memoryWriter;

    WebPData iccChunk, imageChunk, assembledChunk;
    WebPMux * mux = NULL;

    memset(&iccChunk, 0, sizeof(iccChunk));
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
    WebPPictureAlloc(&picture);

    clImageToBGRA8(C, image, (uint8_t *)picture.argb);

    if (!WebPEncode(&config, &picture)) {
        clContextLogError(C, "Failed to encode WebP");
        writeResult = clFalse;
        goto writeCleanup;
    }

    mux = WebPMuxNew();
    iccChunk.bytes = rawProfile.ptr;
    iccChunk.size = rawProfile.size;
    if (!WebPMuxSetChunk(mux, "ICCP", &iccChunk, 0)) {
        clContextLogError(C, "Failed create ICC profile");
        goto writeCleanup;
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
    return writeResult;
}
