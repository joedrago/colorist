// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_IMAGE_H
#define COLORIST_IMAGE_H

#include "colorist/types.h"

typedef struct clProfile clProfile;

typedef struct clImage
{
    int width;
    int height;
    int depth;
    int size;
    uint8_t * pixels;
    clProfile * profile;
} clImage;

clImage * clImageCreate(int width, int height, int depth, clProfile * profile);
void clImageResize(clImage * image, int width, int height, int depth);
void clImageChangeDepth(clImage * image, int depth);
void clImageSetPixel(clImage * image, int x, int y, int r, int g, int b, int a);
void clImageDebugDump(clImage * image, int x, int y, int w, int h);
void clImageDestroy(clImage * image);

clImage * clImageReadPNG(const char * filename);
clBool clImageWritePNG(clImage * image, const char * filename);

clImage * clImageReadJP2(const char * filename);
clBool clImageWriteJP2(clImage * image, const char * filename, int quality, int rate);

clImage * clImageReadJPG(const char * filename);
clBool clImageWriteJPG(clImage * image, const char * filename, int quality);

#endif // ifndef COLORIST_IMAGE_H
