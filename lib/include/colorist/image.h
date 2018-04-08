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
void clImageDebugDump(clImage * image);
void clImageDestroy(clImage * image);

#endif
