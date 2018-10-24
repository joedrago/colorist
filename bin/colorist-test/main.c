// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

int main(int argc, char * argv[])
{
    {
        clContext * C = clContextCreate(NULL);
        clConversionParams params;
        clImage * image;
        clImage * conv;

        clConversionParamsSetDefaults(C, &params);
        params.formatName = "png";

        image = clImageParseString(C, "8x8,(255,0,0)", 8, NULL);
        conv = clImageConvert(C, image, &params);

        clImageDestroy(C, image);
        clImageDestroy(C, conv);
        clContextDestroy(C);
    }
    {
        clContext * C = clContextCreate(NULL);
        clImage * image;

        image = clImageParseString(C, "8x8,(255,0,0)", 8, NULL);
        clImageDebugDump(C, image, 0, 0, 1, 1, 0);

        clImageDestroy(C, image);
        clContextDestroy(C);
    }
    return 0;
}
