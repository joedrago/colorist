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

clImage * clImageReadWebP(struct clContext * C, const char * filename)
{
    clImage * image = NULL;
    clProfile * profile = NULL;
    clRaw fileContents;

    WebPData webpFileContents;
    WebPMux * mux = NULL;
    WebPMuxFrameInfo frameInfo;
    uint32_t muxFlags;

    int width, height;
    uint8_t * readPixels = NULL;

    memset(&frameInfo, 0, sizeof(frameInfo));

    memset(&fileContents, 0, sizeof(fileContents));
    if (!clRawReadFile(C, &fileContents, filename)) {
        goto readCleanup;
    }

    webpFileContents.bytes = fileContents.ptr;
    webpFileContents.size = fileContents.size;
    mux = WebPMuxCreate(&webpFileContents, 0);
    WebPMuxGetFeatures(mux, &muxFlags);
    if (muxFlags & ICCP_FLAG) {
        WebPData iccChunk;
        if (WebPMuxGetChunk(mux, "ICCP", &iccChunk) != WEBP_MUX_OK) {
            clContextLogError(C, "Failed get ICC profile chunk: '%s'", filename);
            goto readCleanup;
        }
        profile = clProfileParse(C, iccChunk.bytes, iccChunk.size, NULL);
        if (!profile) {
            clContextLogError(C, "Failed parse ICC profile chunk: '%s'", filename);
            goto readCleanup;
        }
    }

    if (WebPMuxGetFrame(mux, 1, &frameInfo) != WEBP_MUX_OK) {
        clContextLogError(C, "Failed to get frame chunk in WebP: '%s'", filename);
        goto readCleanup;
    }

    readPixels = WebPDecodeRGBA(frameInfo.bitstream.bytes, frameInfo.bitstream.size, &width, &height);
    if (!readPixels) {
        clContextLogError(C, "Failed to decode WebP: '%s'", filename);
        goto readCleanup;
    }

    image = clImageCreate(C, width, height, 8, profile);
    memcpy(image->pixels, readPixels, 4 * width * height);

readCleanup:
    clRawFree(C, &fileContents);
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

clBool clImageWriteWebP(struct clContext * C, clImage * image, const char * filename, int quality)
{
    clBool writeResult = clTrue;
    clRaw rawProfile;

    cmsUInt32Number srcFormat = (image->depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;
    cmsHTRANSFORM rgbTransform;

    WebPConfig config;
    WebPPicture picture;
    WebPMemoryWriter memoryWriter;
    FILE * f;

    WebPData iccChunk, imageChunk, assembledChunk;
    WebPMux * mux = NULL;

    memset(&rawProfile, 0, sizeof(rawProfile));
    memset(&iccChunk, 0, sizeof(iccChunk));
    memset(&imageChunk, 0, sizeof(imageChunk));
    memset(&assembledChunk, 0, sizeof(assembledChunk));

    WebPMemoryWriterInit(&memoryWriter);
    WebPConfigInit(&config);
    WebPPictureInit(&picture);

    f = fopen(filename, "wb");
    if (!f) {
        clContextLogError(C, "cannot open WebP for write: '%s'", filename);
        writeResult = clFalse;
        goto writeCleanup;
    }

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        clContextLogError(C, "Failed to create ICC profile");
        goto writeCleanup;
    }

    config.lossless = (C->quality >= 100) ? 1 : 0;
    config.emulate_jpeg_size = 1; // consistency across export quality values
    config.quality = (float)C->quality;
    config.method = 6; // always go for the best output, encoding speed be damned

    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = (void *)&memoryWriter;
    picture.use_argb = 1;
    picture.width = image->width;
    picture.height = image->height;
    WebPPictureAlloc(&picture);

    rgbTransform = cmsCreateTransformTHR(C->lcms, image->profile->handle, srcFormat, image->profile->handle, TYPE_BGRA_8, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
    COLORIST_ASSERT(rgbTransform);
    cmsDoTransform(rgbTransform, image->pixels, picture.argb, image->width * image->height);
    cmsDeleteTransform(rgbTransform);

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
        clContextLogError(C, "Failed to assemble WebP: %s", filename);
        goto writeCleanup;
    }

    if (fwrite(assembledChunk.bytes, assembledChunk.size, 1, f) != 1) {
        clContextLogError(C, "Failed to write %d bytes: %s", assembledChunk.size, filename);
        writeResult = clFalse;
        goto writeCleanup;
    }

writeCleanup:
    if (f) {
        fclose(f);
    }
    if (mux) {
        WebPMuxDelete(mux);
    }
    WebPDataClear(&assembledChunk);
    WebPMemoryWriterClear(&memoryWriter);
    WebPPictureFree(&picture);
    clRawFree(C, &rawProfile);
    return writeResult;
}
