// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_IMAGE_H
#define COLORIST_IMAGE_H

#include "colorist/types.h"

struct clContext;
struct clConversionParams;
struct clProfile;
struct clRaw;

typedef struct clImage
{
    int width;
    int height;
    int depth;
    int size;
    uint8_t * pixels;
    struct clProfile * profile;
} clImage;

clImage * clImageCreate(struct clContext * C, int width, int height, int depth, struct clProfile * profile);
clImage * clImageRotate(struct clContext * C, clImage * image, int cwTurns);
clImage * clImageConvert(struct clContext * C, clImage * srcImage, struct clConversionParams * params);
void clImageResize(struct clContext * C, clImage * image, int width, int height, int depth);
void clImageChangeDepth(struct clContext * C, clImage * image, int depth);
void clImageSetPixel(struct clContext * C, clImage * image, int x, int y, int r, int g, int b, int a);
void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent);
void clImageDestroy(struct clContext * C, clImage * image);

clImage * clImageReadJP2(struct clContext * C, const char * filename);
clBool clImageWriteJP2(struct clContext * C, clImage * image, const char * filename, clBool isJ2K, int quality, int rate);

clImage * clImageReadJPG(struct clContext * C, const char * filename);
clBool clImageWriteJPG(struct clContext * C, clImage * image, const char * filename, int quality);
clBool clImageWriteJPGRaw(struct clContext * C, clImage * image, struct clRaw * dst, int quality);
char * clImageWriteJPGURI(struct clContext * C, clImage * image, int quality);

clImage * clImageReadPNG(struct clContext * C, const char * filename);
clBool clImageWritePNG(struct clContext * C, clImage * image, const char * filename);

clImage * clImageReadWebP(struct clContext * C, const char * filename);
clBool clImageWriteWebP(struct clContext * C, clImage * image, const char * filename, int quality);

clImage * clImageParseString(struct clContext * C, const char * str, int depth, struct clProfile * profile);

#endif // ifndef COLORIST_IMAGE_H
