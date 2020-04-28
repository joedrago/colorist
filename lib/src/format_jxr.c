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

// This is the yuckiest.
#define LARGEST_JXR_OUTPUT_SIZE (200 * 1024 * 1024)

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
    clBool scRGB = clFalse;

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
        if (!memcmp(pixelFormat.pGUIDPixFmt, &GUID_PKPixelFormat64bppRGBAHalf, sizeof(GUID_PKPixelFormat64bppRGBAHalf))) {
            clContextLog(C, "jxr", 1, "Decoded 16bpc half-float JXR with no profile, assuming MS Game Bar screencap (scRGB linear @ 80 nits)");

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
            curve.gamma = 1.0f;

            profile = clProfileCreate(C, &primaries, &curve, 80, NULL);

            scRGB = clTrue;
        }
    }

    depth = (pixelFormat.uBitsPerSample > 8) ? 16 : 8;
    if (scRGB) {
        guidPixFormat = GUID_PKPixelFormat128bppRGBAFloat;
    } else {
        guidPixFormat = (depth > 8) ? GUID_PKPixelFormat64bppRGBA : GUID_PKPixelFormat32bppRGBA;
    }

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

    if (!memcmp(&guidPixFormat, &GUID_PKPixelFormat128bppRGBAFloat, sizeof(guidPixFormat))) {
        clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_F32);
        if (Failed(err = pConverter->Copy(pConverter, &rect, (U8 *)image->pixelsF32, image->width * 4 * sizeof(float)))) {
            clContextLogError(C, "Can't copy JXR pixels (F32)");
            clImageDestroy(C, image);
            image = NULL;
            goto readCleanup;
        }
    } else if (!memcmp(&guidPixFormat, &GUID_PKPixelFormat64bppRGBA, sizeof(guidPixFormat))) {
        clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U16);
        if (Failed(err = pConverter->Copy(pConverter, &rect, (U8 *)image->pixelsU16, image->width * 4 * sizeof(uint16_t)))) {
            clContextLogError(C, "Can't copy JXR pixels (U16)");
            clImageDestroy(C, image);
            image = NULL;
            goto readCleanup;
        }
    } else if (!memcmp(&guidPixFormat, &GUID_PKPixelFormat32bppRGBA, sizeof(guidPixFormat))) {
        clImagePrepareWritePixels(C, image, CL_PIXELFORMAT_U8);
        if (Failed(err = pConverter->Copy(pConverter, &rect, (U8 *)image->pixelsU8, image->width * 4 * sizeof(uint8_t)))) {
            clContextLogError(C, "Can't copy JXR pixels (U8)");
            clImageDestroy(C, image);
            image = NULL;
            goto readCleanup;
        }
    } else {
        clContextLogError(C, "Can't copy JXR pixels (Unknown)");
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
    clRaw rawProfile;

    ERR err = WMP_errSuccess;
    PKPixelFormatGUID guidPixFormat;
    CWMIStrCodecParam wmiSCP;
    float fltImageQuality;

    PKFactory * pFactory = NULL;
    struct WMPStream * pEncodeStream = NULL;
    PKCodecFactory * pCodecFactory = NULL;
    PKImageEncode * pEncoder = NULL;

    // This is the worst hack ever.
    clRawRealloc(C, output, LARGEST_JXR_OUTPUT_SIZE);

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
    if (writeParams->quality == 0)
        fltImageQuality = 1.0f;
    else
        fltImageQuality = (float)writeParams->quality / 100.0f;

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        goto cleanup;
    }

    if (Failed(err = PKCreateFactory(&pFactory, PK_SDK_VERSION))) {
        clContextLogError(C, "Can't create JXR PK factory");
        goto cleanup;
    }
    if (Failed(err = pFactory->CreateStreamFromMemory(&pEncodeStream, output->ptr, output->size))) {
        clContextLogError(C, "Can't open JXR file for write");
        goto cleanup;
    }
    if (Failed(err = PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION))) {
        clContextLogError(C, "Can't create JXR codec factory");
        goto cleanup;
    }
    if (Failed(err = pCodecFactory->CreateCodec(&IID_PKImageWmpEncode, &pEncoder))) {
        clContextLogError(C, "Can't create JXR codec");
        goto cleanup;
    }

    if ((wmiSCP.cNumOfSliceMinus1H == 0) && (wmiSCP.uiTileY[0] > 0)) {
        // # of horizontal slices, rounded down by half tile size.
        U32 uTileY = wmiSCP.uiTileY[0] * MB_HEIGHT_PIXEL;
        wmiSCP.cNumOfSliceMinus1H = (U32)image->height < (uTileY >> 1) ? 0 : (image->height + (uTileY >> 1)) / uTileY - 1;
    }
    if ((wmiSCP.cNumOfSliceMinus1V == 0) && (wmiSCP.uiTileX[0] > 0)) {
        // # of vertical slices, rounded down by half tile size.
        U32 uTileX = wmiSCP.uiTileX[0] * MB_HEIGHT_PIXEL;
        wmiSCP.cNumOfSliceMinus1V = (U32)image->width < (uTileX >> 1) ? 0 : (image->width + (uTileX >> 1)) / uTileX - 1;
    }

    if (Failed(err = pEncoder->Initialize(pEncoder, pEncodeStream, &wmiSCP, sizeof(wmiSCP)))) {
        clContextLogError(C, "Can't initialize JXR codec");
        goto cleanup;
    }

    if (fltImageQuality < 1.0F) {
        // if (!args.bOverlapSet) {
        // Image width must be at least 2 MB wide for subsampled chroma and two levels of overlap!
        if ((fltImageQuality >= 0.5F) || (image->width < 2 * MB_WIDTH_PIXEL))
            pEncoder->WMP.wmiSCP.olOverlap = OL_ONE;
        else
            pEncoder->WMP.wmiSCP.olOverlap = OL_TWO;
        // }

        // if (!args.bColorFormatSet) {
        if ((fltImageQuality >= 0.5F) || (image->depth > 8))
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
            if ((fltImageQuality > 0.8f) && (image->depth == 8) && (pEncoder->WMP.wmiSCP.cfColorFormat != YUV_420) &&
                (pEncoder->WMP.wmiSCP.cfColorFormat != YUV_422))
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
        clContextLogError(C, "Can't set pixel format");
        goto cleanup;
    }
    if (Failed(err = pEncoder->SetSize(pEncoder, image->width, image->height))) {
        clContextLogError(C, "Can't set image size");
        goto cleanup;
    }
    if (Failed(err = pEncoder->SetColorContext(pEncoder, rawProfile.ptr, (U32)rawProfile.size))) {
        clContextLogError(C, "Can't set image ICC profile");
        goto cleanup;
    }

    if (image->depth > 8) {
        clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_U16);
        pEncoder->WritePixels(pEncoder, image->height, (U8 *)image->pixelsU16, image->width * 4 * sizeof(uint16_t));
    } else {
        clImagePrepareReadPixels(C, image, CL_PIXELFORMAT_U8);
        pEncoder->WritePixels(pEncoder, image->height, image->pixelsU8, image->width * 4 * sizeof(uint8_t));
    }
    output->size = pEncodeStream->state.buf.cbLast;

    writeResult = clTrue;
cleanup:
    if (pEncoder)
        pEncoder->Release(&pEncoder);
    if (pFactory)
        pFactory->Release(&pFactory);
    clRawFree(C, &rawProfile);
    return writeResult;
}
