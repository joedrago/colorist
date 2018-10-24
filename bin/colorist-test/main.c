// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

int main(int argc, char * argv[])
{
    clContext * C = clContextCreate(NULL);
    clConversionParams params;
    clImage * src;
    clImage * dst;

    {
        C = clContextCreate(NULL);
        clConversionParamsSetDefaults(C, &params);
        params.jobs = 1;

        src = clImageParseString(C, "(255,0,0)", 8, NULL);
        dst = clImageConvert(C, src, &params);

        clImageDestroy(C, src);
        clImageDestroy(C, dst);
        clContextDestroy(C);
    }
    {
        C = clContextCreate(NULL);

        src = clImageParseString(C, "8x8,(255,0,0)", 8, NULL);
        clImageDebugDump(C, src, 0, 0, 1, 1, 0);

        clImageDestroy(C, src);
        clContextDestroy(C);
    }

    printf("colorist-test Complete.\n");
    return 0;
}
