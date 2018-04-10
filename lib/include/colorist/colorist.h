// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_COLORIST_H
#define COLORIST_COLORIST_H

#define COLORIST_VERSION_STRING "0.0.1"
#define COLORIST_VERSION 0x00000001

// Output luminance colorist uses for basic profiles (sRGB, P3, etc)
#define COLORIST_DEFAULT_LUMINANCE 300

#include "colorist/image.h"
#include "colorist/profile.h"

void clDumpVersions();

#endif
