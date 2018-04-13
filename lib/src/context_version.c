// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/version.h"

#include <stdio.h>

#include "jpeglib.h"
#include "lcms2.h"
#include "openjpeg.h"
#include "png.h"
#include "zlib.h"

void clContextPrintVersions(clContext * C)
{
    clContextLog(C, NULL, 0, "Versions:");
    clContextLog(C, NULL, 1, "colorist: %s", COLORIST_VERSION_STRING);
    clContextLog(C, NULL, 1, "jpeglib : %d", JPEG_LIB_VERSION);
    clContextLog(C, NULL, 1, "lcms2   : %d", LCMS_VERSION);
    clContextLog(C, NULL, 1, "libpng  : %s", PNG_LIBPNG_VER_STRING);
    clContextLog(C, NULL, 1, "openjpeg: %s", opj_version());
    clContextLog(C, NULL, 1, "zlib    : %s", ZLIB_VERSION);
    clContextLog(C, NULL, 0, "");
}
