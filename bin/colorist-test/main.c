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
        clImage * image = clImageParseString(C, "(255,0,0)", 8, NULL);
        clImageDestroy(C, image);
        clContextDestroy(C);
    }
    return 0;
}
