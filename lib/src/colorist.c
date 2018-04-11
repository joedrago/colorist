// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include "jpeglib.h"
#include "lcms2.h"
#include "openjpeg.h"
#include "png.h"
#include "zlib.h"

void clDumpVersions()
{
    printf("Versions:\n");
    printf(" * colorist: %s\n", COLORIST_VERSION_STRING);
    printf(" * jpeglib : %d\n", JPEG_LIB_VERSION);
    printf(" * lcms2   : %d\n", LCMS_VERSION);
    printf(" * libpng  : %s\n", PNG_LIBPNG_VER_STRING);
    printf(" * openjpeg: %s\n", opj_version());
    printf(" * zlib    : %s\n", ZLIB_VERSION);
    printf("\n");
}
