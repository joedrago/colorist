// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include "zlib.h"
#include "png.h"
#include "openjpeg.h"
#include "lcms2.h"

void clDumpVersions()
{
    printf("Versions:\n");
    printf(" * colorist: %s\n", COLORIST_VERSION_STRING);
    printf(" * zlib    : %s\n", ZLIB_VERSION);
    printf(" * libpng  : %s\n", PNG_LIBPNG_VER_STRING);
    printf(" * openjpeg: %s\n", opj_version());
    printf(" * lcms2   : %d\n", LCMS_VERSION);
    printf("\n");
}
