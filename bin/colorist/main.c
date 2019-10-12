// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include "cJSON.h"

#ifdef WIN32_MEMORY_LEAK_DETECTION
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clContextSilentLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(section);
    COLORIST_UNUSED(indent);
    COLORIST_UNUSED(format);
    COLORIST_UNUSED(args);
}

static cJSON * errorJSON = NULL;
static void clContextSilentLogError(clContext * C, const char * format, va_list args)
{
    int needed;
    char * buffer;
    needed = vsnprintf(NULL, 0, format, args);
    if (needed <= 0) {
        return;
    }

    buffer = clAllocate(needed + 1);
    vsnprintf(buffer, needed + 1, format, args);

    if (errorJSON) {
        cJSON_Delete(errorJSON);
    }
    errorJSON = cJSON_CreateObject();
    cJSON_AddStringToObject(errorJSON, "error", buffer);

    clFree(buffer);
}

int main(int argc, char * argv[])
{
    int ret = 1;
    clContext * C;
    clContextSystem system;
    int i;
    cJSON * jsonOutput = NULL;

    system.alloc = clContextDefaultAlloc;
    system.free = clContextDefaultFree;
    system.log = clContextDefaultLog;
    system.error = clContextDefaultLogError;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--json")) {
            // JSON output enabled, avoid any other text output
            system.log = clContextSilentLog;
            system.error = clContextSilentLogError;
            jsonOutput = cJSON_CreateObject();
            break;
        }
    }

#ifdef WIN32_MEMORY_LEAK_DETECTION
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(116);
#endif

    C = clContextCreate(&system);
    if (!clContextParseArgs(C, argc, (const char **)argv)) {
        goto cleanup;
    }
    if (C->help || (C->action == CL_ACTION_NONE)) {
        clContextPrintSyntax(C);
        goto cleanup;
    }
    if (C->verbose)
        clContextPrintArgs(C);

    switch (C->action) {
        case CL_ACTION_CALC:
            ret = clContextGenerate(C, jsonOutput);
            break;
        case CL_ACTION_CONVERT:
            ret = clContextConvert(C);
            break;
        case CL_ACTION_GENERATE:
            ret = clContextGenerate(C, NULL);
            break;
        case CL_ACTION_IDENTIFY:
            ret = clContextIdentify(C, jsonOutput);
            break;
        case CL_ACTION_MODIFY:
            ret = clContextModify(C);
            break;
        case CL_ACTION_ERROR:
        case CL_ACTION_NONE:
        default:
            clContextLogError(C, "Unimplemented action: %s", clActionToString(C, C->action));
            break;
    }

cleanup:

    if (jsonOutput) {
        cJSON * which = errorJSON ? errorJSON : jsonOutput;
        char * textOutput = cJSON_PrintUnformatted(which);
        printf("%s\n", textOutput);
        free(textOutput);
        cJSON_Delete(jsonOutput);
        jsonOutput = NULL;
    }
    if (errorJSON) {
        cJSON_Delete(errorJSON);
        errorJSON = NULL;
    }

    clContextDestroy(C);
    return ret;
}
