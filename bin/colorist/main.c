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
#endif
#include <crtdbg.h>
#include <stdlib.h>
#include <string.h>

static void clContextSilentLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
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

#ifdef COLORIST_EMSCRIPTEN
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE
#endif
int execute(int argc, char * argv[])
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
    if (!clContextParseArgs(C, argc, argv)) {
        goto cleanup;
    }
    if (C->help || (C->action == CL_ACTION_NONE)) {
        clContextPrintSyntax(C);
        clContextDestroy(C);
        goto cleanup;
    }
    if (C->verbose)
        clContextPrintArgs(C);

#ifdef COLORIST_EMSCRIPTEN
    EM_ASM(
        Module.outputFilename = null;
        );
#endif

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
        case CL_ACTION_REPORT:
            ret = clContextReport(C);
            break;
        default:
            clContextLogError(C, "Unimplemented action: %s", clActionToString(C, C->action));
            break;
    }

cleanup:
#ifdef COLORIST_EMSCRIPTEN
    if ((ret == 0) && C->outputFilename) {
        EM_ASM_({
            Module.outputFilename = UTF8ToString($0);
        }, C->outputFilename);
    }
    EM_ASM(
        if (Module.onExecuteFinished) {
        setTimeout(function() {
            Module.onExecuteFinished();
        }, 0);
    }
        );
#endif

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

int main(int argc, char * argv[])
{
#ifdef COLORIST_EMSCRIPTEN
    return 0;
#else
    return execute(argc, argv);
#endif
}
