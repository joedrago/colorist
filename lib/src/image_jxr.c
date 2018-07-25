// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/raw.h"

#include "JXRGlue.h"

// Y, U, V, YHP, UHP, VHP
int DPK_QPS_420[11][6] = { // for 8 bit only
    { 66, 65, 70, 72, 72, 77 },
    { 59, 58, 63, 64, 63, 68 },
    { 52, 51, 57, 56, 56, 61 },
    { 48, 48, 54, 51, 50, 55 },
    { 43, 44, 48, 46, 46, 49 },
    { 37, 37, 42, 38, 38, 43 },
    { 26, 28, 31, 27, 28, 31 },
    { 16, 17, 22, 16, 17, 21 },
    { 10, 11, 13, 10, 10, 13 },
    { 5, 5, 6, 5, 5, 6 },
    { 2, 2, 3, 2, 2, 2 }
};

int DPK_QPS_8[12][6] = {
    { 67, 79, 86, 72, 90, 98 },
    { 59, 74, 80, 64, 83, 89 },
    { 53, 68, 75, 57, 76, 83 },
    { 49, 64, 71, 53, 70, 77 },
    { 45, 60, 67, 48, 67, 74 },
    { 40, 56, 62, 42, 59, 66 },
    { 33, 49, 55, 35, 51, 58 },
    { 27, 44, 49, 28, 45, 50 },
    { 20, 36, 42, 20, 38, 44 },
    { 13, 27, 34, 13, 28, 34 },
    { 7, 17, 21, 8, 17, 21 }, // Photoshop 100%
    { 2, 5, 6, 2, 5, 6 }
};

int DPK_QPS_16[11][6] = {
    { 197, 203, 210, 202, 207, 213 },
    { 174, 188, 193, 180, 189, 196 },
    { 152, 167, 173, 156, 169, 174 },
    { 135, 152, 157, 137, 153, 158 },
    { 119, 137, 141, 119, 138, 142 },
    { 102, 120, 125, 100, 120, 124 },
    { 82, 98, 104, 79, 98, 103 },
    { 60, 76, 81, 58, 76, 81 },
    { 39, 52, 58, 36, 52, 58 },
    { 16, 27, 33, 14, 27, 33 },
    { 5, 8, 9, 4, 7, 8 }
};

clImage * clImageReadJXR(struct clContext * C, const char * filename)
{
    clRaw rawProfile;
    clProfile * profile = NULL;
    clImage * image = NULL;
    ERR err = WMP_errSuccess;
    int profileByteCount = 0;

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
        clContextLogError(C, "Can't create JXR PK factory: %s", filename);
        goto cleanup;
    }

    if (Failed(err = PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION))) {
        clContextLogError(C, "Can't create JXR codec factory: %s", filename);
        goto cleanup;
    }

    if (Failed(err = pCodecFactory->CreateDecoderFromFile(filename, &pDecoder))) {
        clContextLogError(C, "Can't create JXR codec factory: %s", filename);
        goto cleanup;
    }

    if ((pDecoder->uWidth < 1) || (pDecoder->uHeight < 1)) {
        clContextLogError(C, "Invalid JXR image size (%ux%u): %s", pDecoder->uWidth, pDecoder->uHeight, filename);
        goto cleanup;
    }

    pixelFormat.pGUIDPixFmt = &pDecoder->guidPixFormat;
    if (Failed(err = PixelFormatLookup(&pixelFormat, LOOKUP_FORWARD))) {
        clContextLogError(C, "Unrecognized pixel format (1): %s", filename);
        goto cleanup;
    }
    if (Failed(err = PixelFormatLookup(&pixelFormat, LOOKUP_BACKWARD_TIF))) {
        clContextLogError(C, "Unrecognized pixel format (2): %s", filename);
        goto cleanup;
    }

    pDecoder->GetColorContext(pDecoder, NULL, &profileByteCount);
    if (profileByteCount > 0) {
        clRawRealloc(C, &rawProfile, profileByteCount);
        if (Failed(err = pDecoder->GetColorContext(pDecoder, rawProfile.ptr, &profileByteCount))) {
            clContextLogError(C, "Can't read JXR format's ICC profile: %s", filename);
            goto cleanup;
        }
        profile = clProfileParse(C, rawProfile.ptr, rawProfile.size, NULL);
        if (!profile) {
            clContextLogError(C, "Invalid ICC profile in JXR: %s", filename);
            goto cleanup;
        }
    }

    if (pixelFormat.uBitsPerSample > 8) {
        depth = 16;
        guidPixFormat = GUID_PKPixelFormat64bppRGBA;
    } else {
        depth = 8;
        guidPixFormat = GUID_PKPixelFormat32bppRGBA;
    }

    if (Failed(err = pDecoder->GetFrameCount(pDecoder, &frameCount)) || (frameCount < 1)) {
        clContextLogError(C, "Invalid JXR frame count (%d): %s", frameCount, filename);
        goto cleanup;
    }

    if (Failed(err = pCodecFactory->CreateFormatConverter(&pConverter))) {
        clContextLogError(C, "Can't create JXR format converter: %s", filename);
        goto cleanup;
    }

    if (Failed(err = pConverter->Initialize(pConverter, pDecoder, "jxr", guidPixFormat))) {
        clContextLogError(C, "Can't initialize JXR format converter: %s", filename);
        goto cleanup;
    }

    rect.Width = pDecoder->uWidth;
    rect.Height = pDecoder->uHeight;

    clImageLogCreate(C, pDecoder->uWidth, pDecoder->uHeight, depth, profile);
    image = clImageCreate(C, pDecoder->uWidth, pDecoder->uHeight, depth, profile);
    if (Failed(err = pConverter->Copy(pConverter, &rect, image->pixels, image->width * 4 * clDepthToBytes(C, image->depth)))) {
        clContextLogError(C, "Can't copy JXR pixels: %s", filename);
        clImageDestroy(C, image);
        image = NULL;
        goto cleanup;
    }

cleanup:
    if (profile)
        clProfileDestroy(C, profile);
    if (pConverter)
        pConverter->Release(&pConverter);
    if (pDecoder)
        pDecoder->Release(&pDecoder);
    if (pFactory)
        pFactory->Release(&pFactory);
    clRawFree(C, &rawProfile);
    return image;
}

clBool clImageWriteJXR(struct clContext * C, clImage * image, const char * filename, int quality)
{
    clBool result = clFalse;
    clRaw rawProfile;

    ERR err = WMP_errSuccess;
    PKPixelFormatGUID guidPixFormat;
    CWMIStrCodecParam wmiSCP;
    float fltImageQuality;

    PKFactory * pFactory = NULL;
    struct WMPStream * pEncodeStream = NULL;
    PKCodecFactory * pCodecFactory = NULL;
    PKImageEncode * pEncoder = NULL;

    // Defaults
    guidPixFormat = (image->depth > 8) ? GUID_PKPixelFormat64bppRGBA : GUID_PKPixelFormat32bppRGBA;
    memset(&wmiSCP, 0, sizeof(wmiSCP));
    wmiSCP.bVerbose = FALSE;
    wmiSCP.cfColorFormat = YUV_444;
    wmiSCP.bdBitDepth = BD_LONG;
    wmiSCP.bfBitstreamFormat = FREQUENCY;
    wmiSCP.bProgressiveMode = TRUE;
    wmiSCP.olOverlap = OL_ONE;
    wmiSCP.cNumOfSliceMinus1H = wmiSCP.cNumOfSliceMinus1V = 0;
    wmiSCP.sbSubband = SB_ALL;
    wmiSCP.uAlphaMode = 2;
    wmiSCP.uiDefaultQPIndex = 1;
    wmiSCP.uiDefaultQPIndexAlpha = 1;
    if (quality == 0)
        fltImageQuality = 1.0f;
    else
        fltImageQuality = (float)quality / 100.0f;

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        goto cleanup;
    }

    if (Failed(err = PKCreateFactory(&pFactory, PK_SDK_VERSION))) {
        clContextLogError(C, "Can't create JXR PK factory: %s", filename);
        goto cleanup;
    }
    if (Failed(err = pFactory->CreateStreamFromFilename(&pEncodeStream, filename, "wb"))) {
        clContextLogError(C, "Can't open JXR file for write: %s", filename);
        goto cleanup;
    }
    if (Failed(err = PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION))) {
        clContextLogError(C, "Can't create JXR codec factory: %s", filename);
        goto cleanup;
    }
    if (Failed(err = pCodecFactory->CreateCodec(&IID_PKImageWmpEncode, &pEncoder))) {
        clContextLogError(C, "Can't create JXR codec: %s", filename);
        goto cleanup;
    }

    if (( wmiSCP.cNumOfSliceMinus1H == 0) && ( wmiSCP.uiTileY[0] > 0) ) {
        // # of horizontal slices, rounded down by half tile size.
        U32 uTileY = wmiSCP.uiTileY[0] * MB_HEIGHT_PIXEL;
        wmiSCP.cNumOfSliceMinus1H = (U32)image->height < (uTileY >> 1) ? 0 : (image->height + (uTileY >> 1)) / uTileY - 1;
    }
    if (( wmiSCP.cNumOfSliceMinus1V == 0) && ( wmiSCP.uiTileX[0] > 0) ) {
        // # of vertical slices, rounded down by half tile size.
        U32 uTileX = wmiSCP.uiTileX[0] * MB_HEIGHT_PIXEL;
        wmiSCP.cNumOfSliceMinus1V = (U32)image->width < (uTileX >> 1) ? 0 : (image->width + (uTileX >> 1)) / uTileX - 1;
    }

    if (Failed(err = pEncoder->Initialize(pEncoder, pEncodeStream, &wmiSCP, sizeof(wmiSCP)))) {
        clContextLogError(C, "Can't initialize JXR codec: %s", filename);
        goto cleanup;
    }

    if (fltImageQuality < 1.0F) {
        // if (!args.bOverlapSet) {
        // Image width must be at least 2 MB wide for subsampled chroma and two levels of overlap!
        if (( fltImageQuality >= 0.5F) || ( image->width < 2 * MB_WIDTH_PIXEL) )
            pEncoder->WMP.wmiSCP.olOverlap = OL_ONE;
        else
            pEncoder->WMP.wmiSCP.olOverlap = OL_TWO;
        // }

        // if (!args.bColorFormatSet) {
        if (( fltImageQuality >= 0.5F) || (image->depth > 8))
            pEncoder->WMP.wmiSCP.cfColorFormat = YUV_444;
        else
            pEncoder->WMP.wmiSCP.cfColorFormat = YUV_420;
        // }

        // if (PI.bdBitDepth == BD_1) {
        //     pEncoder->WMP.wmiSCP.uiDefaultQPIndex = (U8)(8 - 5.0F * fltImageQuality + 0.5F);
        // } else
        {
            // remap [0.8, 0.866, 0.933, 1.0] to [0.8, 0.9, 1.0, 1.1]
            // to use 8-bit DPK QP table (0.933 == Photoshop JPEG 100)
            int qi;
            float qf;
            int * pQPs;
            if ((fltImageQuality > 0.8f) && (image->depth == 8) &&
                (pEncoder->WMP.wmiSCP.cfColorFormat != YUV_420) &&
                (pEncoder->WMP.wmiSCP.cfColorFormat != YUV_422) )
                fltImageQuality = 0.8f + (fltImageQuality - 0.8f) * 1.5f;

            qi = (int)(10.f * fltImageQuality);
            qf = 10.f * fltImageQuality - (float)qi;

            if ((pEncoder->WMP.wmiSCP.cfColorFormat == YUV_420) || (pEncoder->WMP.wmiSCP.cfColorFormat == YUV_422)) {
                pQPs = DPK_QPS_420[qi];
            } else {
                pQPs = (image->depth == 8) ? DPK_QPS_8[qi] : DPK_QPS_16[qi];
            }

            pEncoder->WMP.wmiSCP.uiDefaultQPIndex = (U8)(0.5f + (float)pQPs[0] * (1.f - qf) + (float)(pQPs + 6)[0] * qf);
            pEncoder->WMP.wmiSCP.uiDefaultQPIndexU = (U8)(0.5f + (float)pQPs[1] * (1.f - qf) + (float)(pQPs + 6)[1] * qf);
            pEncoder->WMP.wmiSCP.uiDefaultQPIndexV = (U8)(0.5f + (float)pQPs[2] * (1.f - qf) + (float)(pQPs + 6)[2] * qf);
            pEncoder->WMP.wmiSCP.uiDefaultQPIndexYHP = (U8)(0.5f + (float)pQPs[3] * (1.f - qf) + (float)(pQPs + 6)[3] * qf);
            pEncoder->WMP.wmiSCP.uiDefaultQPIndexUHP = (U8)(0.5f + (float)pQPs[4] * (1.f - qf) + (float)(pQPs + 6)[4] * qf);
            pEncoder->WMP.wmiSCP.uiDefaultQPIndexVHP = (U8)(0.5f + (float)pQPs[5] * (1.f - qf) + (float)(pQPs + 6)[5] * qf);
        }
    } else {
        pEncoder->WMP.wmiSCP.uiDefaultQPIndex = (U8)fltImageQuality;
    }

    if (pEncoder->WMP.wmiSCP.uAlphaMode == 2)
        pEncoder->WMP.wmiSCP_Alpha.uiDefaultQPIndex = wmiSCP.uiDefaultQPIndexAlpha;

    if (Failed(err = pEncoder->SetPixelFormat(pEncoder, guidPixFormat))) {
        clContextLogError(C, "Can't set pixel format: %s", filename);
        goto cleanup;
    }
    if (Failed(err = pEncoder->SetSize(pEncoder, image->width, image->height))) {
        clContextLogError(C, "Can't set image size: %s", filename);
        goto cleanup;
    }
    if (Failed(err = pEncoder->SetColorContext(pEncoder, rawProfile.ptr, rawProfile.size))) {
        clContextLogError(C, "Can't set image ICC profile: %s", filename);
        goto cleanup;
    }

    pEncoder->WritePixels(pEncoder, image->height, image->pixels, image->width * 4 * clDepthToBytes(C, image->depth));
    result = clTrue;
cleanup:
    if (pEncoder)
        pEncoder->Release(&pEncoder);
    if (pFactory)
        pFactory->Release(&pFactory);
    clRawFree(C, &rawProfile);
    return result;
}
