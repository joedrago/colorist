// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include "colorist/image.h"

clImage * readImage(const char * filename, Format * outFormat)
{
    clImage * image = NULL;
    Format format = detectFormat(filename);
    if (outFormat)
        *outFormat = format;
    if (format == FORMAT_ERROR) {
        return NULL;
    }
    switch (format) {
        case FORMAT_JP2:
            image = clImageReadJP2(filename);
            break;
        case FORMAT_JPG:
            image = clImageReadJPG(filename);
            break;
        case FORMAT_PNG:
            image = clImageReadPNG(filename);
            break;
        default:
            clLogError("Unimplemented file reader '%s'", formatToString(format));
            break;
    }
    return image;
}
