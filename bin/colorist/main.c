#include "colorist/colorist.h"

#include <stdio.h>

int main(int argc, char * argv[])
{
    {
        clImage * image = clImageCreate(2, 2, 8, NULL);
        clImageSetPixel(image, 0, 0, 255, 128, 238, 0);
        clImageDebugDump(image);
        clImageChangeDepth(image, 16);
        clImageDebugDump(image);
        clImageChangeDepth(image, 8);
        clImageDebugDump(image);
        clImageDestroy(image);
    }
    {
        clImage * image;
        clProfile * profile;
        profile = clProfileParse((const uint8_t *)"foo", 3);
        clProfileDestroy(profile);
    }
    return 0;
}
