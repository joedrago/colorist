// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/raw.h"

#include <string.h>

clBool clFormatDetectAVIF(struct clContext * C, struct clFormat * format, struct clRaw * input);
struct clImage * clFormatReadAVIF(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteAVIF(struct clContext * C,
                         struct clImage * image,
                         const char * formatName,
                         struct clRaw * output,
                         struct clWriteParams * writeParams);

struct clImage * clFormatReadBMP(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteBMP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadJPG(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteJPG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadJP2(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteJP2(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadJXR(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteJXR(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadPNG(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWritePNG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadTIFF(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteTIFF(struct clContext * C,
                         struct clImage * image,
                         const char * formatName,
                         struct clRaw * output,
                         struct clWriteParams * writeParams);

struct clImage * clFormatReadWebP(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteWebP(struct clContext * C,
                         struct clImage * image,
                         const char * formatName,
                         struct clRaw * output,
                         struct clWriteParams * writeParams);

static clBool detectFormatSignature(struct clContext * C, struct clFormat * format, struct clRaw * input)
{
    COLORIST_UNUSED(C);

    int signatureIndex;
    for (signatureIndex = 0; signatureIndex < CL_FORMAT_MAX_SIGNATURES; ++signatureIndex) {
        const unsigned char * signature = format->signatures[signatureIndex];
        size_t signatureLength = format->signatureLengths[signatureIndex];
        if (signatureLength == 0) {
            continue;
        }
        if (signatureLength > input->size) {
            continue;
        }
        if (signature && !memcmp(signature, input->ptr, signatureLength)) {
            return clTrue;
        }
    }
    return clFalse;
}

void clContextRegisterBuiltinFormats(struct clContext * C)
{
    // AVIF
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "avif";
        format.description = "AVIF";
        format.mimeType = "image/avif";
        format.extensions[0] = "avif";
        format.extensions[1] = "avifs";
        format.depth = CL_FORMAT_DEPTH_8_OR_10_OR_12;
        format.usesQuality = clTrue;
        format.usesRate = clFalse;
        format.usesYUVFormat = clTrue;
        format.detectFunc = clFormatDetectAVIF;
        format.readFunc = clFormatReadAVIF;
        format.writeFunc = clFormatWriteAVIF;
        clContextRegisterFormat(C, &format);
    }

    // BMP
    {
        static const unsigned char bmpSig[2] = { 0x42, 0x4D };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "bmp";
        format.description = "BMP";
        format.mimeType = "image/bmp";
        format.extensions[0] = "bmp";
        format.signatures[0] = bmpSig;
        format.signatureLengths[0] = sizeof(bmpSig);
        format.depth = CL_FORMAT_DEPTH_8_OR_10;
        format.usesQuality = clFalse;
        format.usesRate = clFalse;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadBMP;
        format.writeFunc = clFormatWriteBMP;
        clContextRegisterFormat(C, &format);
    }

    // JPG
    {
        static const unsigned char jpgSig[2] = { 0xFF, 0xD8 };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "jpg";
        format.description = "JPEG";
        format.mimeType = "image/jpeg";
        format.extensions[0] = "jpg";
        format.extensions[1] = "jpeg";
        format.signatures[0] = jpgSig;
        format.signatureLengths[0] = sizeof(jpgSig);
        format.depth = CL_FORMAT_DEPTH_8;
        format.usesQuality = clTrue;
        format.usesRate = clFalse;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadJPG;
        format.writeFunc = clFormatWriteJPG;
        clContextRegisterFormat(C, &format);
    }

    // JP2
    {
        static const unsigned char jp2Sig[8] = { 0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20 };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "jp2";
        format.description = "JPEG2000 (JP2)";
        format.mimeType = "image/jp2";
        format.extensions[0] = "jp2";
        format.signatures[0] = jp2Sig;
        format.signatureLengths[0] = sizeof(jp2Sig);
        format.depth = CL_FORMAT_DEPTH_8_TO_16;
        format.usesQuality = clTrue;
        format.usesRate = clTrue;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadJP2;
        format.writeFunc = clFormatWriteJP2;
        clContextRegisterFormat(C, &format);
    }

    // J2K
    {
        static const unsigned char j2kSig[4] = { 0xFF, 0x4F, 0xFF, 0x51 };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "j2k";
        format.description = "JPEG2000 (J2K)";
        format.mimeType = "image/jp2";
        format.extensions[0] = "j2k";
        format.signatures[0] = j2kSig;
        format.signatureLengths[0] = sizeof(j2kSig);
        format.depth = CL_FORMAT_DEPTH_8_TO_16;
        format.usesQuality = clTrue;
        format.usesRate = clTrue;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadJP2;
        format.writeFunc = clFormatWriteJP2;
        clContextRegisterFormat(C, &format);
    }

    // JXR
    {
        // static const unsigned char j2kSig[4] = { 0xFF, 0x4F, 0xFF, 0x51 };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "jxr";
        format.description = "JPEG-XR (JXR)";
        format.mimeType = "image/jxr";
        format.extensions[0] = "jxr";
        // format.signatures[0] = j2kSig;
        // format.signatureLengths[0] = sizeof(j2kSig);
        format.depth = CL_FORMAT_DEPTH_8_TO_16;
        format.usesQuality = clTrue;
        format.usesRate = clFalse;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadJXR;
        format.writeFunc = clFormatWriteJXR;
        clContextRegisterFormat(C, &format);
    }

    // PNG
    {
        static const unsigned char pngSig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "png";
        format.description = "PNG";
        format.mimeType = "image/png";
        format.extensions[0] = "png";
        format.signatures[0] = pngSig;
        format.signatureLengths[0] = sizeof(pngSig);
        format.depth = CL_FORMAT_DEPTH_8_OR_16;
        format.usesQuality = clFalse;
        format.usesRate = clFalse;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadPNG;
        format.writeFunc = clFormatWritePNG;
        clContextRegisterFormat(C, &format);
    }

    // TIFF
    {
        static const unsigned char tiffSig0[4] = { 0x49, 0x49, 0x2A, 0x00 };
        static const unsigned char tiffSig1[4] = { 0x4D, 0x4D, 0x00, 0x2A };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "tiff";
        format.description = "TIFF";
        format.mimeType = "image/tiff";
        format.extensions[0] = "tiff";
        format.extensions[1] = "tif";
        format.signatures[0] = tiffSig0;
        format.signatureLengths[0] = sizeof(tiffSig0);
        format.signatures[1] = tiffSig1;
        format.signatureLengths[1] = sizeof(tiffSig1);
        format.depth = CL_FORMAT_DEPTH_8_OR_16_OR_32;
        format.usesQuality = clFalse;
        format.usesRate = clFalse;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadTIFF;
        format.writeFunc = clFormatWriteTIFF;
        clContextRegisterFormat(C, &format);
    }

    // WebP
    {
        static const unsigned char webpSig[12] = { 0x52, 0x49, 0x46, 0x46, 0x80, 0x04, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50 };

        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "webp";
        format.description = "WebP";
        format.mimeType = "image/webp";
        format.extensions[0] = "webp";
        format.signatures[0] = webpSig;
        format.signatureLengths[0] = sizeof(webpSig);
        format.depth = CL_FORMAT_DEPTH_8;
        format.usesQuality = clTrue;
        format.usesRate = clFalse;
        format.usesYUVFormat = clFalse;
        format.detectFunc = detectFormatSignature;
        format.readFunc = clFormatReadWebP;
        format.writeFunc = clFormatWriteWebP;
        clContextRegisterFormat(C, &format);
    }
}
