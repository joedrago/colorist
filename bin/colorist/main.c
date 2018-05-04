// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#ifdef WIN32_MEMORY_LEAK_DETECTION
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

int main(int argc, char * argv[])
{
    int ret = 1;
    clContext * C;

#ifdef WIN32_MEMORY_LEAK_DETECTION
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(116);
#endif

    C = clContextCreate(NULL);
    if (!clContextParseArgs(C, argc, argv)) {
        return 1;
    }
    if (C->help || (C->action == CL_ACTION_NONE)) {
        clContextPrintSyntax(C);
        clContextDestroy(C);
        return 0;
    }
    if (C->verbose)
        clContextPrintArgs(C);

    switch (C->action) {
        case CL_ACTION_CONVERT:
            ret = clContextConvert(C);
            break;
        case CL_ACTION_GENERATE:
            ret = clContextGenerate(C);
            break;
        case CL_ACTION_IDENTIFY:
            ret = clContextIdentify(C);
            break;
        case CL_ACTION_MODIFY:
            ret = clContextModify(C);
            break;
        case CL_ACTION_REPORT:
            ret = clContextReport(C);
            break;
        default:
            clContextLogError(C, "Unimplemented action: %s", clActionToString(C, C->action));
            break;
    }
    clContextDestroy(C);
    return ret;
}
