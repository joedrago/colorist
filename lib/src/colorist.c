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
    clLog(NULL, 0, "Versions:");
    clLog(NULL, 1, "colorist: %s", COLORIST_VERSION_STRING);
    clLog(NULL, 1, "jpeglib : %d", JPEG_LIB_VERSION);
    clLog(NULL, 1, "lcms2   : %d", LCMS_VERSION);
    clLog(NULL, 1, "libpng  : %s", PNG_LIBPNG_VER_STRING);
    clLog(NULL, 1, "openjpeg: %s", opj_version());
    clLog(NULL, 1, "zlib    : %s", ZLIB_VERSION);
    clLog(NULL, 0, "");
}
