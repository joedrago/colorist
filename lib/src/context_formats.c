// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include <string.h>

struct clImage * clFormatReadBMP(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteBMP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadJPG(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteJPG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadJP2(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteJP2(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadPNG(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWritePNG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadTIFF(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteTIFF(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadWebP(struct clContext * C, const char * formatName, struct clRaw * input);
clBool clFormatWriteWebP(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

void clContextRegisterBuiltinFormats(struct clContext * C)
{
    // BMP
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "bmp";
        format.description = "BMP";
        format.mimeType = "image/bmp";
        format.extensions[0] = "bmp";
        format.depth = CL_FORMAT_DEPTH_8_OR_10;
        format.usesQuality = clFalse;
        format.usesRate = clFalse;
        format.readFunc = clFormatReadBMP;
        format.writeFunc = clFormatWriteBMP;
        clContextRegisterFormat(C, &format);
    }

    // JPG
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "jpg";
        format.description = "JPEG";
        format.mimeType = "image/jpeg";
        format.extensions[0] = "jpg";
        format.extensions[1] = "jpeg";
        format.depth = CL_FORMAT_DEPTH_8;
        format.usesQuality = clTrue;
        format.usesRate = clFalse;
        format.readFunc = clFormatReadJPG;
        format.writeFunc = clFormatWriteJPG;
        clContextRegisterFormat(C, &format);
    }

    // JP2
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "jp2";
        format.description = "JPEG2000 (JP2)";
        format.mimeType = "image/jp2";
        format.extensions[0] = "jp2";
        format.depth = CL_FORMAT_DEPTH_8_TO_16;
        format.usesQuality = clTrue;
        format.usesRate = clTrue;
        format.readFunc = clFormatReadJP2;
        format.writeFunc = clFormatWriteJP2;
        clContextRegisterFormat(C, &format);
    }

    // J2K
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "j2k";
        format.description = "JPEG2000 (J2K)";
        format.mimeType = "image/jp2";
        format.extensions[0] = "j2k";
        format.depth = CL_FORMAT_DEPTH_8_TO_16;
        format.usesQuality = clTrue;
        format.usesRate = clTrue;
        format.readFunc = clFormatReadJP2;
        format.writeFunc = clFormatWriteJP2;
        clContextRegisterFormat(C, &format);
    }

    // PNG
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "png";
        format.description = "PNG";
        format.mimeType = "image/png";
        format.extensions[0] = "png";
        format.depth = CL_FORMAT_DEPTH_8_OR_16;
        format.usesQuality = clFalse;
        format.usesRate = clFalse;
        format.readFunc = clFormatReadPNG;
        format.writeFunc = clFormatWritePNG;
        clContextRegisterFormat(C, &format);
    }

    // TIFF
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "tiff";
        format.description = "TIFF";
        format.mimeType = "image/tiff";
        format.extensions[0] = "tiff";
        format.extensions[1] = "tif";
        format.depth = CL_FORMAT_DEPTH_8_OR_16;
        format.usesQuality = clFalse;
        format.usesRate = clFalse;
        format.readFunc = clFormatReadTIFF;
        format.writeFunc = clFormatWriteTIFF;
        clContextRegisterFormat(C, &format);
    }

    // WebP
    {
        clFormat format;
        memset(&format, 0, sizeof(format));
        format.name = "webp";
        format.description = "WebP";
        format.mimeType = "image/webp";
        format.extensions[0] = "webp";
        format.depth = CL_FORMAT_DEPTH_8;
        format.usesQuality = clTrue;
        format.usesRate = clFalse;
        format.readFunc = clFormatReadWebP;
        format.writeFunc = clFormatWriteWebP;
        clContextRegisterFormat(C, &format);
    }
}
