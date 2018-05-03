// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/profile.h"
#include "colorist/task.h"

#include <string.h>
#include <stdlib.h>

// ------------------------------------------------------------------------------------------------
// Stock Primaries

typedef struct StockPrimaries
{
    const char * name;
    clProfilePrimaries primaries;
} StockPrimaries;

static StockPrimaries stockPrimaries[] = {
    { "bt709", { { 0.64f, 0.33f }, { 0.30f, 0.60f }, { 0.15f, 0.06f }, { 0.3127f, 0.3290f } } },
    { "bt2020", { { 0.708f, 0.292f }, { 0.170f, 0.797f }, { 0.131f, 0.046f }, { 0.3127f, 0.3290f } } },
    { "p3", { { 0.68f, 0.32f }, { 0.265f, 0.690f }, { 0.150f, 0.060f }, { 0.3127f, 0.3290f } } }
};
static const unsigned int stockPrimariesCount = sizeof(stockPrimaries) / sizeof(stockPrimaries[0]);

// ------------------------------------------------------------------------------------------------
// clAction

clAction clActionFromString(struct clContext * C, const char * str)
{
    if (!strcmp(str, "identify")) return CL_ACTION_IDENTIFY;
    if (!strcmp(str, "id")) return CL_ACTION_IDENTIFY;
    if (!strcmp(str, "generate")) return CL_ACTION_GENERATE;
    if (!strcmp(str, "gen")) return CL_ACTION_GENERATE;
    if (!strcmp(str, "convert")) return CL_ACTION_CONVERT;
    if (!strcmp(str, "report")) return CL_ACTION_REPORT;
    return CL_ACTION_ERROR;
}

const char * clActionToString(struct clContext * C, clAction action)
{
    switch (action) {
        case CL_ACTION_NONE:     return "--";
        case CL_ACTION_IDENTIFY: return "identify";
        case CL_ACTION_GENERATE: return "generate";
        case CL_ACTION_CONVERT:  return "convert";
        case CL_ACTION_REPORT:   return "report";
        case CL_ACTION_ERROR:
        default:
            break;
    }
    return "Unknown";
}

// ------------------------------------------------------------------------------------------------
// clFormat

clFormat clFormatFromString(struct clContext * C, const char * str)
{
    if (!strcmp(str, "auto")) return CL_FORMAT_AUTO;
    if (!strcmp(str, "icc")) return CL_FORMAT_ICC;
    if (!strcmp(str, "jp2")) return CL_FORMAT_JP2;
    if (!strcmp(str, "jpg")) return CL_FORMAT_JPG;
    if (!strcmp(str, "png")) return CL_FORMAT_PNG;
    if (!strcmp(str, "webp")) return CL_FORMAT_WEBP;
    return CL_FORMAT_ERROR;
}

const char * clFormatToString(struct clContext * C, clFormat format)
{
    switch (format) {
        case CL_FORMAT_AUTO:  return "Auto";
        case CL_FORMAT_ICC:   return "ICC";
        case CL_FORMAT_JP2:   return "JP2";
        case CL_FORMAT_JPG:   return "JPG";
        case CL_FORMAT_PNG:   return "PNG";
        case CL_FORMAT_WEBP:  return "WebP";
        case CL_FORMAT_ERROR:
        default:
            break;
    }
    return "Unknown";
}

clFormat clFormatDetect(struct clContext * C, const char * filename)
{
    const char * ext = strrchr(filename, '.');
    if (ext == NULL) {
        clContextLogError(C, "Unable to guess format");
        return CL_FORMAT_ERROR;
    }
    ++ext; // skip past the period
    if (!strcmp(ext, "icc")) return CL_FORMAT_ICC;
    if (!strcmp(ext, "j2k")) return CL_FORMAT_J2K;
    if (!strcmp(ext, "jp2")) return CL_FORMAT_JP2;
    if (!strcmp(ext, "jpg")) return CL_FORMAT_JPG;
    if (!strcmp(ext, "png")) return CL_FORMAT_PNG;
    if (!strcmp(ext, "webp")) return CL_FORMAT_WEBP;
    clContextLogError(C, "Unknown file extension '%s'", ext);
    return CL_FORMAT_ERROR;
}

int clFormatMaxDepth(struct clContext * C, clFormat format)
{
    switch (format) {
        case CL_FORMAT_J2K:   return 16;
        case CL_FORMAT_JP2:   return 16;
        case CL_FORMAT_JPG:   return 8;
        case CL_FORMAT_PNG:   return 16;
        case CL_FORMAT_WEBP:  return 8;
        default:
            break;
    }
    clContextLogError(C, "clFormatMaxDepth() called on unknown format");
    return 8;
}

// ------------------------------------------------------------------------------------------------
// clTonemap

clTonemap clTonemapFromString(struct clContext * C, const char * str)
{
    if (!strcmp(str, "on")) return CL_TONEMAP_ON;
    if (!strcmp(str, "yes")) return CL_TONEMAP_ON;
    if (!strcmp(str, "enabled")) return CL_TONEMAP_ON;

    if (!strcmp(str, "off")) return CL_TONEMAP_OFF;
    if (!strcmp(str, "no")) return CL_TONEMAP_OFF;
    if (!strcmp(str, "disabled")) return CL_TONEMAP_OFF;

    return CL_TONEMAP_AUTO;
}

const char * clTonemapToString(struct clContext * C, clTonemap tonemap)
{
    switch (tonemap) {
        case CL_TONEMAP_ON: return "on";
        case CL_TONEMAP_OFF: return "on";
        default:
            break;
    }
    return "auto";
}

// ------------------------------------------------------------------------------------------------
// clContext

static void clConversionParamsSetOutputProfileDefaults(clContext * C, clConversionParams * params)
{
    params->autoGrade = clFalse;
    params->copyright = NULL;
    params->description = NULL;
    params->gamma = 0;
    params->luminance = 0;
    memset(params->primaries, 0, sizeof(float) * 8);
}

void clConversionParamsSetDefaults(clContext * C, clConversionParams * params)
{
    clConversionParamsSetOutputProfileDefaults(C, params);
    params->bpp = 0;
    params->format = CL_FORMAT_AUTO;
    params->jobs = clTaskLimit();
    params->iccOverrideOut = NULL;
    params->quality = 90; // ?
    params->rate = 150;   // ?
    params->rect[0] = 0;
    params->rect[1] = 0;
    params->rect[2] = -1;
    params->rect[3] = -1;
    params->tonemap = CL_TONEMAP_AUTO;
}

clContext * clContextCreate(clContextSystem * system)
{
    clContextAllocFunc alloc;
    clContext * C;

    // bootstrap!
    alloc = clContextDefaultAlloc;
    if (system && system->alloc)
        alloc = system->alloc;
    C = (clContext *)alloc(NULL, sizeof(clContext));

    C->system.alloc = clContextDefaultAlloc;
    C->system.free = clContextDefaultFree;
    C->system.log = clContextDefaultLog;
    C->system.error = clContextDefaultLogError;
    if (system) {
        if (system->alloc)
            C->system.alloc = system->alloc;
        if (system->free)
            C->system.free = system->free;
        if (system->log)
            C->system.log = system->log;
        if (system->error)
            C->system.error = system->error;
    }

    // TODO: hook up memory management plugin to route through C->system.alloc
    C->lcms = cmsCreateContext(NULL, NULL);

    // Clue in LittleCMS that we intend to do absolute colorimetric conversions
    // on profiles that use white points other than D50 (profiles containing a
    // chromatic adaptation tag). Setting this to 0 causes absolute conversions
    // to fully honor the chad tags in the profiles (if any).
    cmsSetAdaptationStateTHR(C->lcms, 0);

    // Default args
    C->action = CL_ACTION_NONE;
    clConversionParamsSetDefaults(C, &C->params);
    C->help = clFalse;
    C->iccOverrideIn = NULL;
    C->verbose = clFalse;
    C->inputFilename = NULL;
    C->outputFilename = NULL;

    return C;
}

void clContextDestroy(clContext * C)
{
    cmsDeleteContext(C->lcms);
    clFree(C);
}

clBool clContextGetStockPrimaries(struct clContext * C, const char * name, struct clProfilePrimaries * outPrimaries)
{
    unsigned int index;
    for (index = 0; index < stockPrimariesCount; ++index) {
        if (!strcmp(name, stockPrimaries[index].name)) {
            memcpy(outPrimaries, &stockPrimaries[index].primaries, sizeof(clProfilePrimaries));
            return clTrue;
        }
    }
    return clFalse;
}

clBool clContextGetRawStockPrimaries(struct clContext * C, const char * name, float outPrimaries[8])
{
    unsigned int index;
    for (index = 0; index < stockPrimariesCount; ++index) {
        if (!strcmp(name, stockPrimaries[index].name)) {
            memcpy(outPrimaries, &stockPrimaries[index].primaries, sizeof(float) * 8);
            return clTrue;
        }
    }
    return clFalse;
}

// ------------------------------------------------------------------------------------------------
// Memory management

char * clContextStrdup(clContext * C, const char * str)
{
    int len = strlen(str);
    char * dup = clAllocate(len + 1);
    memcpy(dup, str, len);
    dup[len] = 0;
    return dup;
}

// ------------------------------------------------------------------------------------------------
// Argument parsing

static clBool parsePrimaries(clContext * C, float primaries[8], const char * arg)
{
    char * buffer;
    char * token;
    int index;
    clProfilePrimaries stockPrimaries;
    if (clContextGetStockPrimaries(C, arg, &stockPrimaries)) {
        primaries[0] = stockPrimaries.red[0];
        primaries[1] = stockPrimaries.red[1];
        primaries[2] = stockPrimaries.green[0];
        primaries[3] = stockPrimaries.green[1];
        primaries[4] = stockPrimaries.blue[0];
        primaries[5] = stockPrimaries.blue[1];
        primaries[6] = stockPrimaries.white[0];
        primaries[7] = stockPrimaries.white[1];
        return clTrue;
    }
    buffer = clContextStrdup(C, arg);
    index = 0;
    for (token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (index >= 8) {
            clContextLogError(C, "Too many primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)");
            return clFalse;
        }
        primaries[index] = (float)strtod(token, NULL);
        ++index;
    }
    if (index < 8) {
        clContextLogError(C, "Too few primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)");
        return clFalse;
    }
    return clTrue;
}

static clBool parseRect(clContext * C, int rect[4], const char * arg)
{
    char * buffer = clContextStrdup(C, arg);
    char * token;
    int index = 0;
    for (token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (index >= 8) {
            clContextLogError(C, "Too many values for rect: (expecting: x,y,w,h)");
            return clFalse;
        }
        rect[index] = atoi(token);
        ++index;
    }
    if (index < 4) {
        clContextLogError(C, "Too few values for rect: (expecting: x,y,w,h)");
        return clFalse;
    }
    return clTrue;
}

#define NEXTARG()                                                     \
    if (((argIndex + 1) == argc) || (argv[argIndex + 1][0] == '-')) { \
        clContextLogError(C, "-%c requires an argument.", arg[1]);    \
        return clFalse;                                               \
    }                                                                 \
    arg = argv[++argIndex]

static clBool validateArgs(clContext * C);

clBool clContextParseArgs(clContext * C, int argc, char * argv[])
{
    int argIndex = 1;
    const char * filenames[2] = { NULL, NULL };
    int taskLimit = clTaskLimit();
    while (argIndex < argc) {
        const char * arg = argv[argIndex];
        if ((arg[0] == '-')) {
            switch (arg[1]) {
                case 'a':
                    C->params.autoGrade = clTrue;
                    break;
                case 'b':
                    NEXTARG();
                    C->params.bpp = atoi(arg);
                    break;
                case 'c':
                    NEXTARG();
                    C->params.copyright = arg;
                    break;
                case 'd':
                    NEXTARG();
                    C->params.description = arg;
                    break;
                case 'f':
                    NEXTARG();
                    C->params.format = clFormatFromString(C, arg);
                    if (C->params.format == CL_FORMAT_ERROR) {
                        clContextLogError(C, "Unknown format: %s", arg);
                        return clFalse;
                    }
                    break;
                case 'g':
                    NEXTARG();
                    if (arg[0] == 's') {
                        C->params.gamma = -1.0f; // Use source gamma
                    } else {
                        C->params.gamma = (float)strtod(arg, NULL);
                    }
                    break;
                case 'h':
                    C->help = clTrue;
                    break;
                case 'i':
                    NEXTARG();
                    C->iccOverrideIn = arg;
                    break;
                case 'j':
                    NEXTARG();
                    C->params.jobs = atoi(arg);
                    if (C->params.jobs == 0)
                        C->params.jobs = taskLimit;
                    C->params.jobs = CL_CLAMP(C->params.jobs, 1, taskLimit);
                    break;
                case 'l':
                    NEXTARG();
                    if (arg[0] == 's') {
                        C->params.luminance = -1; // Use source luminance
                    } else {
                        C->params.luminance = atoi(arg);
                    }
                    break;
                case 'o':
                    NEXTARG();
                    C->params.iccOverrideOut = arg;
                    break;
                case 'p':
                    NEXTARG();
                    if (!parsePrimaries(C, C->params.primaries, arg))
                        return clFalse;
                    break;
                case 'q':
                    NEXTARG();
                    C->params.quality = atoi(arg);
                    break;
                case 'r':
                    NEXTARG();
                    C->params.rate = atoi(arg);
                    break;
                case 't':
                    NEXTARG();
                    C->params.tonemap = clTonemapFromString(C, arg);
                    break;
                case 'v':
                    C->verbose = clTrue;
                    break;
                case 'z':
                    NEXTARG();
                    if (!parseRect(C, C->params.rect, arg))
                        return clFalse;
                    break;
            }
        } else {
            if (C->action == CL_ACTION_NONE) {
                C->action = clActionFromString(C, arg);
                if (C->action == CL_ACTION_ERROR) {
                    clContextLogError(C, "unknown action '%s', expecting convert, identify, generate, or report", arg);
                    return clFalse;
                }
            } else if (filenames[0] == NULL) {
                filenames[0] = arg;
            } else if (filenames[1] == NULL) {
                filenames[1] = arg;
            } else {
                clContextLogError(C, "Too many positional arguments.");
                return clFalse;
            }
        }
        ++argIndex;
    }

    switch (C->action) {
        case CL_ACTION_IDENTIFY:
            C->inputFilename = filenames[0];
            if (!C->inputFilename) {
                clContextLogError(C, "identify requires an input filename.");
                return clFalse;
            }
            if (filenames[1]) {
                clContextLogError(C, "identify does not accept an output filename.");
                return clFalse;
            }
            break;

        case CL_ACTION_GENERATE:
            if (filenames[0] && filenames[1]) {
                C->inputFilename = filenames[0];
                C->outputFilename = filenames[1];
            } else if (filenames[0]) {
                C->outputFilename = filenames[0];
            }
            if (!C->outputFilename) {
                clContextLogError(C, "generate requires an output filename.");
                return clFalse;
            }
            break;

        case CL_ACTION_CONVERT:
            C->inputFilename = filenames[0];
            if (!C->inputFilename) {
                clContextLogError(C, "convert requires an input filename.");
                return clFalse;
            }
            C->outputFilename = filenames[1];
            if (!C->outputFilename) {
                clContextLogError(C, "convert requires an output filename.");
                return clFalse;
            }
            break;

        case CL_ACTION_REPORT:
            C->inputFilename = filenames[0];
            if (!C->inputFilename) {
                clContextLogError(C, "report requires an input filename.");
                return clFalse;
            }
            C->outputFilename = filenames[1];
            if (!C->outputFilename) {
                clContextLogError(C, "report requires an output filename.");
                return clFalse;
            }
            break;

        default:
            break;
    }
    return validateArgs(C);
}

static clBool validateArgs(clContext * C)
{
    clBool valid = clTrue;
    if ((C->params.bpp != 0) && (C->params.bpp != 8) && (C->params.bpp != 16)) {
        clContextLogError(C, "Unknown bpp: %d", C->params.bpp);
        valid = clFalse;
    }
    if (C->params.autoGrade && (C->params.gamma != 0.0f) && (C->params.luminance != 0)) {
        clContextLog(C, "syntax", 0, "WARNING: auto color grading mode (-a) is useless with both -g and -l specified, disabling auto color grading");
        C->params.autoGrade = clFalse;
    }
    if (C->params.iccOverrideOut) {
        clContextLog(C, "syntax", 0, "-o in use, disabling all other output profile options");
        clConversionParamsSetOutputProfileDefaults(C, &C->params);
    }
    return valid;
}

void clContextPrintArgs(clContext * C)
{
    clContextLog(C, "syntax", 0, "Args:");
    clContextLog(C, "syntax", 1, "Action     : %s", clActionToString(C, C->action));
    if (C->params.bpp)
        clContextLog(C, "syntax", 1, "bpp        : %d", C->params.bpp);
    else
        clContextLog(C, "syntax", 1, "bpp        : auto");
    clContextLog(C, "syntax", 1, "copyright  : %s", C->params.copyright ? C->params.copyright : "--");
    clContextLog(C, "syntax", 1, "description: %s", C->params.description ? C->params.description : "--");
    clContextLog(C, "syntax", 1, "format     : %s", clFormatToString(C, C->params.format));
    if (C->params.gamma < 0.0f) {
        clContextLog(C, "syntax", 1, "gamma      : source gamma (forced)");
    } else if (C->params.gamma > 0.0f)
        clContextLog(C, "syntax", 1, "gamma      : %g", C->params.gamma);
    else
        clContextLog(C, "syntax", 1, "gamma      : auto");
    clContextLog(C, "syntax", 1, "help       : %s", C->help ? "enabled" : "disabled");
    clContextLog(C, "syntax", 1, "ICC in     : %s", C->iccOverrideIn ? C->iccOverrideIn : "--");
    clContextLog(C, "syntax", 1, "ICC out    : %s", C->params.iccOverrideOut ? C->params.iccOverrideOut : "--");
    if (C->params.luminance < 0) {
        clContextLog(C, "syntax", 1, "luminance  : source luminance (forced)");
    } else if (C->params.luminance) {
        clContextLog(C, "syntax", 1, "luminance  : %d", C->params.luminance);
    } else {
        clContextLog(C, "syntax", 1, "luminance  : auto");
    }
    if (C->params.primaries[0] > 0.0f)
        clContextLog(C, "syntax", 1, "primaries  : r:(%.4g,%.4g) g:(%.4g,%.4g) b:(%.4g,%.4g) w:(%.4g,%.4g)",
            C->params.primaries[0], C->params.primaries[1],
            C->params.primaries[2], C->params.primaries[3],
            C->params.primaries[4], C->params.primaries[5],
            C->params.primaries[6], C->params.primaries[7]);
    else
        clContextLog(C, "syntax", 1, "primaries  : auto");
    clContextLog(C, "syntax", 1, "rect       : (%d,%d) %dx%d", C->params.rect[0], C->params.rect[1], C->params.rect[2], C->params.rect[3]);
    clContextLog(C, "syntax", 1, "tonemap    : %s", clTonemapToString(C, C->params.tonemap));
    clContextLog(C, "syntax", 1, "verbose    : %s", C->verbose ? "enabled" : "disabled");
    clContextLog(C, "syntax", 1, "input      : %s", C->inputFilename ? C->inputFilename : "--");
    clContextLog(C, "syntax", 1, "output     : %s", C->outputFilename ? C->outputFilename : "--");
}

void clContextPrintSyntax(clContext * C)
{
    clContextLog(C, NULL, 0, "Syntax: colorist convert  [input]        [output]       [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist identify [input]                       [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist generate                [output.icc]   [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist generate [image string] [output image] [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist report   [input]        [output.html]  [OPTIONS]");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Basic Options:");
    clContextLog(C, NULL, 0, "    -h             : Display this help");
    clContextLog(C, NULL, 0, "    -j JOBS        : Number of jobs to use when working. 0 for as many as possible (default)");
    clContextLog(C, NULL, 0, "    -v             : Verbose mode.");
    clContextLog(C, NULL, 0, "    -z x,y,w,h     : Pixels to dump in identify mode. x,y,w,h");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Input Options:");
    clContextLog(C, NULL, 0, "    -i file.icc    : Override source ICC profile. default is to use embedded profile (if any), or sRGB@300");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Output Profile Options:");
    clContextLog(C, NULL, 0, "    -o file.icc    : Override destination ICC profile. Disables all other output profile options");
    clContextLog(C, NULL, 0, "    -a             : Enable automatic color grading of max luminance and gamma (disabled by default)");
    clContextLog(C, NULL, 0, "    -c COPYRIGHT   : ICC profile copyright string.");
    clContextLog(C, NULL, 0, "    -d DESCRIPTION : ICC profile description.");
    clContextLog(C, NULL, 0, "    -g GAMMA       : Output gamma. 0 for auto (default), or \"source\" to force source gamma");
    clContextLog(C, NULL, 0, "    -l LUMINANCE   : ICC profile max luminance. 0 for auto (default), or \"source\" to force source luminance");
    clContextLog(C, NULL, 0, "    -p PRIMARIES   : Color primaries. Use builtin (bt709, bt2020, p3) or in the form: rx,ry,gx,gy,bx,by,wx,wy");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Output Format Options:");
    clContextLog(C, NULL, 0, "    -b BPP         : Output bits-per-pixel. 8, 16, or 0 for auto (default)");
    clContextLog(C, NULL, 0, "    -f FORMAT      : Output format. auto (default), icc, j2k, jp2, jpg, png, webp");
    clContextLog(C, NULL, 0, "    -q QUALITY     : Output quality for JPG and WebP. JP2 can also use it (see -r below). (default: 90)");
    clContextLog(C, NULL, 0, "    -r RATE        : Output rate for JP2. If 0, JP2 codec uses -q value above instead. (default: 150)");
    clContextLog(C, NULL, 0, "    -t TONEMAP     : Set tonemapping. auto (default), on, or off");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "See image string examples here: https://joedrago.github.io/colorist/docs/Usage.html");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "CPUs Available: %d", clTaskLimit());
    clContextLog(C, NULL, 0, "");
    clContextPrintVersions(C);
}
