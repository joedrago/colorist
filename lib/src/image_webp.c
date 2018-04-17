// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include "encode.h"

#include <string.h>

clImage * clImageReadWebP(struct clContext * C, const char * filename)
{
    clImage * image = NULL;
    clProfile * profile = NULL;
    return image;
}

clBool clImageWriteWebP(struct clContext * C, clImage * image, const char * filename, int quality)
{
    return clFalse;
}
