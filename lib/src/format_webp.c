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

struct clImage * clFormatReadWebP(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteWebP(struct clContext * C,
                         struct clImage * image,
                         const char * formatName,
                         struct clRaw * output,
                         struct clWriteParams * writeParams);

struct clImage * clFormatReadWebP(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);

    clImage * image = NULL;
    clProfile * profile = NULL;

    Timer t;
    timerStart(&t);

    WebPData webpFileContents;
    webpFileContents.bytes = input->ptr;
    webpFileContents.size = input->size;
    WebPMux * mux = WebPMuxCreate(&webpFileContents, 0);

    uint32_t muxFlags;
    WebPMuxGetFeatures(mux, &muxFlags);

    if (overrideProfile) {
        profile = clProfileClone(C, overrideProfile);
    } else if (muxFlags & ICCP_FLAG) {
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
    if (!WebPGetInfo(frameInfo.bitstream.bytes, frameInfo.bitstream.size, &width, &height)) {
        clContextLogError(C, "Failed to decode WebP");
        goto readCleanup;
    }

    clImageLogCreate(C, width, height, 8, profile);
    image = clImageCreate(C, width, height, 8, profile);
    clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U8);
    if (!WebPDecodeRGBAInto(frameInfo.bitstream.bytes,
                            frameInfo.bitstream.size,
                            image->pixelsU8,
                            image->width * image->height * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_U8),
                            image->width * CL_BYTES_PER_PIXEL(CL_PIXELFORMAT_U8))) {
        clContextLogError(C, "Failed to decode WebP");
        goto readCleanup;
    }

    C->readExtraInfo.decodeCodecSeconds = timerElapsedSeconds(&t);

readCleanup:
    WebPDataClear(&frameInfo.bitstream);
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
    return writeResult;
}
