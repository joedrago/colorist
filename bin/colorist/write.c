// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include "colorist/image.h"

clBool writeImage(clImage * image, const char * filename, Format format, int quality, int rate)
{
    if (format == FORMAT_AUTO) {
        format = detectFormat(filename);
        if (format == FORMAT_ERROR) {
            clLogError("Unknown output file format '%s', please specify with -f", filename);
            return clFalse;
        }
    }
    switch (format) {
        case FORMAT_JP2:
            return clImageWriteJP2(image, filename, quality, rate);
            break;
        case FORMAT_JPG:
            return clImageWriteJPG(image, filename, quality);
            break;
        case FORMAT_PNG:
            return clImageWritePNG(image, filename);
            break;
        default:
            clLogError("Unimplemented file writer '%s'", formatToString(format));
            break;
    }
    return clFalse;
}
