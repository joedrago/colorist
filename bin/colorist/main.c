// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include "colorist/task.h"

#include <string.h>

typedef struct StockPrimaries
{
    const char * name;
    float primaries[8];
} StockPrimaries;

static StockPrimaries stockPrimaries[] = {
    { "bt709", { 0.64f, 0.33f, 0.30f, 0.60f, 0.15f, 0.06f, 0.3127f, 0.3290f } },
    { "bt2020", { 0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f } },
    { "p3", { 0.68f, 0.32f, 0.265f, 0.690f, 0.150f, 0.060f, 0.3127f, 0.3290f } }
};
static const unsigned int stockPrimariesCount = sizeof(stockPrimaries) / sizeof(stockPrimaries[0]);

static void setDefaults(Args * args)
{
    args->action = ACTION_NONE;
    args->autoGrade = clFalse;
    args->bpp = 0;
    args->copyright = NULL;
    args->description = NULL;
    args->format = FORMAT_AUTO;
    args->gamma = 0;
    args->help = clFalse;
    args->jobs = clTaskLimit();
    args->luminance = 0;
    memset(args->primaries, 0, sizeof(float) * 8);
    args->quality = 90; // ?
    args->rate = 150;   // ?
    args->verbose = clFalse;
    args->rect[0] = 0;
    args->rect[1] = 0;
    args->rect[2] = 3;
    args->rect[3] = 3;
    args->tonemap = TONEMAP_AUTO;
    args->inputFilename = NULL;
    args->outputFilename = NULL;
}

static Action stringToAction(const char * str)
{
    if (!strcmp(str, "identify")) return ACTION_IDENTIFY;
    if (!strcmp(str, "id"))       return ACTION_IDENTIFY;
    if (!strcmp(str, "generate")) return ACTION_GENERATE;
    if (!strcmp(str, "gen"))      return ACTION_GENERATE;
    if (!strcmp(str, "convert"))  return ACTION_CONVERT;
    return ACTION_ERROR;
}

static const char * actionToString(Action action)
{
    switch (action) {
        case ACTION_NONE:     return "--";
        case ACTION_IDENTIFY: return "identify";
        case ACTION_GENERATE: return "generate";
        case ACTION_CONVERT:  return "convert";
        case ACTION_ERROR:
        default:
            break;
    }
    return "Unknown";
}

Format stringToFormat(const char * str)
{
    if (!strcmp(str, "auto")) return FORMAT_AUTO;
    if (!strcmp(str, "icc"))  return FORMAT_ICC;
    if (!strcmp(str, "jp2"))  return FORMAT_JP2;
    if (!strcmp(str, "jpg"))  return FORMAT_JPG;
    if (!strcmp(str, "png"))  return FORMAT_PNG;
    return FORMAT_ERROR;
}

const char * formatToString(Format format)
{
    switch (format) {
        case FORMAT_AUTO: return "Auto";
        case FORMAT_ICC:  return "ICC";
        case FORMAT_JP2:  return "JP2";
        case FORMAT_JPG:  return "JPG";
        case FORMAT_PNG:  return "PNG";
        case FORMAT_ERROR:
        default:
            break;
    }
    return "Unknown";
}

Format detectFormat(const char * filename)
{
    const char * ext = strrchr(filename, '.');
    if (ext == NULL) {
        clLogError("Unable to guess format");
        return FORMAT_ERROR;
    }
    ++ext; // skip past the period
    if (!strcmp(ext, "icc")) return FORMAT_ICC;
    if (!strcmp(ext, "jp2")) return FORMAT_JP2;
    if (!strcmp(ext, "jpg")) return FORMAT_JPG;
    if (!strcmp(ext, "png")) return FORMAT_PNG;
    clLogError("Unknown file extension '%s'", ext);
    return FORMAT_ERROR;
}

static clBool parsePrimaries(float primaries[8], const char * arg)
{
    char * buffer;
    char * token;
    unsigned int index;
    for (index = 0; index < stockPrimariesCount; ++index) {
        if (!strcmp(arg, stockPrimaries[index].name)) {
            memcpy(primaries, stockPrimaries[index].primaries, sizeof(float) * 8);
            return clTrue;
        }
    }
    buffer = strdup(arg);
    index = 0;
    for (token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (index >= 8) {
            clLogError("Too many primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)");
            return clFalse;
        }
        primaries[index] = (float)strtod(token, NULL);
        ++index;
    }
    if (index < 8) {
        clLogError("Too few primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)");
        return clFalse;
    }
    return clTrue;
}

static clBool parseRect(int rect[4], const char * arg)
{
    char * buffer = strdup(arg);
    char * token;
    int index = 0;
    for (token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (index >= 8) {
            clLogError("Too many values for rect: (expecting: x,y,w,h)");
            return clFalse;
        }
        rect[index] = atoi(token);
        ++index;
    }
    if (index < 4) {
        clLogError("Too few values for rect: (expecting: x,y,w,h)");
        return clFalse;
    }
    return clTrue;
}

#define NEXTARG()                                                     \
    if (((argIndex + 1) == argc) || (argv[argIndex + 1][0] == '-')) { \
        clLogError("-%c requires an argument.", arg[1]);              \
        return clFalse;                                               \
    }                                                                 \
    arg = argv[++argIndex]

static clBool parseArgs(Args * args, int argc, char * argv[])
{
    int argIndex = 1;
    const char * filenames[2] = { NULL, NULL };
    int taskLimit = clTaskLimit();
    while (argIndex < argc) {
        const char * arg = argv[argIndex];
        if ((arg[0] == '-')) {
            switch (arg[1]) {
                case 'a':
                    args->autoGrade = clTrue;
                    break;
                case 'b':
                    NEXTARG();
                    args->bpp = atoi(arg);
                    break;
                case 'c':
                    NEXTARG();
                    args->copyright = arg;
                    break;
                case 'd':
                    NEXTARG();
                    args->description = arg;
                    break;
                case 'f':
                    NEXTARG();
                    args->format = stringToFormat(arg);
                    if (args->format == FORMAT_ERROR) {
                        clLogError("Unknown format: %s", arg);
                        return clFalse;
                    }
                    break;
                case 'g':
                    NEXTARG();
                    if (arg[0] == 's') {
                        args->gamma = -1.0f; // Use source gamma
                    } else {
                        args->gamma = (float)strtod(arg, NULL);
                    }
                    break;
                case 'h':
                    args->help = clTrue;
                    break;
                case 'j':
                    NEXTARG();
                    args->jobs = atoi(arg);
                    if (args->jobs == 0)
                        args->jobs = taskLimit;
                    args->jobs = CL_CLAMP(args->jobs, 1, taskLimit);
                    break;
                case 'l':
                    NEXTARG();
                    if (arg[0] == 's') {
                        args->luminance = -1; // Use source luminance
                    } else {
                        args->luminance = atoi(arg);
                    }
                    break;
                case 'p':
                    NEXTARG();
                    if (!parsePrimaries(args->primaries, arg))
                        return clFalse;
                    break;
                case 'q':
                    NEXTARG();
                    args->quality = atoi(arg);
                    break;
                case 'r':
                    NEXTARG();
                    args->rate = atoi(arg);
                    break;
                case 't':
                    NEXTARG();
                    if (!strcmp(arg, "auto")) {
                        args->tonemap = TONEMAP_AUTO;
                    } else if (!strcmp(arg, "off")) {
                        args->tonemap = TONEMAP_OFF;
                    } else if (!strcmp(arg, "0")) {
                        args->tonemap = TONEMAP_OFF;
                    } else if (!strcmp(arg, "disabled")) {
                        args->tonemap = TONEMAP_OFF;
                    } else if (!strcmp(arg, "on")) {
                        args->tonemap = TONEMAP_ON;
                    } else if (!strcmp(arg, "1")) {
                        args->tonemap = TONEMAP_ON;
                    } else if (!strcmp(arg, "enabled")) {
                        args->tonemap = TONEMAP_ON;
                    }
                    break;
                case 'v':
                    args->verbose = clTrue;
                    break;
                case 'z':
                    NEXTARG();
                    if (!parseRect(args->rect, arg))
                        return clFalse;
                    break;
            }
        } else {
            if (args->action == ACTION_NONE) {
                args->action = stringToAction(arg);
                if (args->action == ACTION_ERROR) {
                    clLogError("unknown action '%s', expecting identify or convert", arg);
                    return clFalse;
                }
            } else if (filenames[0] == NULL) {
                filenames[0] = arg;
            } else if (filenames[1] == NULL) {
                filenames[1] = arg;
            } else {
                clLogError("Too many positional arguments.");
                return clFalse;
            }
        }
        ++argIndex;
    }

    switch (args->action) {
        case ACTION_IDENTIFY:
            args->inputFilename = filenames[0];
            if (!args->inputFilename) {
                clLogError("identify requires an input filename.");
                return clFalse;
            }
            if (filenames[1]) {
                clLogError("identify does not accept an output filename.");
                return clFalse;
            }
            break;

        case ACTION_GENERATE:
            args->outputFilename = filenames[0];
            if (!args->outputFilename) {
                clLogError("generate requires an output filename.");
                return clFalse;
            }
            if (filenames[1]) {
                clLogError("generate does not accept both an input and output filename.");
                return clFalse;
            }
            break;

        case ACTION_CONVERT:
            args->inputFilename = filenames[0];
            if (!args->inputFilename) {
                clLogError("convert requires an input filename.");
                return clFalse;
            }
            args->outputFilename = filenames[1];
            if (!args->outputFilename) {
                clLogError("convert requires an output filename.");
                return clFalse;
            }
            break;

        default:
            break;
    }
    return clTrue;
}

static clBool validateArgs(Args * args)
{
    clBool valid = clTrue;
    if ((args->bpp != 0) && (args->bpp != 8) && (args->bpp != 16)) {
        clLogError("Unknown bpp: %d", args->bpp);
        valid = clFalse;
    }
    if (args->autoGrade && (args->gamma != 0.0f) && (args->luminance != 0)) {
        clLog("syntax", 0, "WARNING: auto color grading mode (-a) is useless with both -g and -l specified, disabling auto color grading");
        args->autoGrade = clFalse;
    }
    return valid;
}

static void dumpArgs(Args * args)
{
    clLog("syntax", 0, "Args:");
    clLog("syntax", 1, "Action     : %s", actionToString(args->action));
    if (args->bpp)
        clLog("syntax", 1, "bpp        : %d", args->bpp);
    else
        clLog("syntax", 1, "bpp        : auto");
    clLog("syntax", 1, "copyright  : %s", args->copyright ? args->copyright : "--");
    clLog("syntax", 1, "description: %s", args->description ? args->description : "--");
    clLog("syntax", 1, "format     : %s", formatToString(args->format));
    if (args->gamma < 0.0f) {
        clLog("syntax", 1, "gamma      : source gamma (forced)");
    } else if (args->gamma > 0.0f)
        clLog("syntax", 1, "gamma      : %g", args->gamma);
    else
        clLog("syntax", 1, "gamma      : auto");
    clLog("syntax", 1, "help       : %s", args->help ? "enabled" : "disabled");
    if (args->luminance < 0) {
        clLog("syntax", 1, "luminance  : source luminance (forced)");
    } else if (args->luminance) {
        clLog("syntax", 1, "luminance  : %d", args->luminance);
    } else {
        clLog("syntax", 1, "luminance  : auto");
    }
    if (args->primaries[0] > 0.0f)
        clLog("syntax", 1, "primaries  : r:(%.4g,%.4g) g:(%.4g,%.4g) b:(%.4g,%.4g) w:(%.4g,%.4g)",
            args->primaries[0], args->primaries[1],
            args->primaries[2], args->primaries[3],
            args->primaries[4], args->primaries[5],
            args->primaries[6], args->primaries[7]);
    else
        clLog("syntax", 1, "primaries  : auto");
    clLog("syntax", 1, "rect       : (%d,%d) %dx%d", args->rect[0], args->rect[1], args->rect[2], args->rect[3]);
    clLog("syntax", 1, "verbose    : %s", args->verbose ? "enabled" : "disabled");
    clLog("syntax", 1, "input      : %s", args->inputFilename ? args->inputFilename : "--");
    clLog("syntax", 1, "output     : %s", args->outputFilename ? args->outputFilename : "--");
}

static void printSyntax()
{
    clLog(NULL, 0, "         colorist convert  [input] [output] [OPTIONS]");
    clLog(NULL, 0, "Syntax : colorist identify [input]          [OPTIONS]");
    clLog(NULL, 0, "         colorist generate         [output] [OPTIONS]");
    clLog(NULL, 0, "Options:");
    clLog(NULL, 0, "    -a             : Enable automatic color grading of max luminance and gamma (disabled by default)");
    clLog(NULL, 0, "    -b BPP         : Output bits-per-pixel. 8, 16, or 0 for auto (default)");
    clLog(NULL, 0, "    -c COPYRIGHT   : ICC profile copyright string.");
    clLog(NULL, 0, "    -d DESCRIPTION : ICC profile description.");
    clLog(NULL, 0, "    -f FORMAT      : Output format. auto (default), icc, jp2, jpg, png");
    clLog(NULL, 0, "    -g GAMMA       : Output gamma. 0 for auto (default), or \"source\" to force source gamma");
    clLog(NULL, 0, "    -h             : Display this help");
    clLog(NULL, 0, "    -j JOBS        : Number of jobs to use when color grading. 0 for as many as possible (default)");
    clLog(NULL, 0, "    -l LUMINANCE   : ICC profile max luminance. 0 for auto (default), or \"source\" to force source luminance");
    clLog(NULL, 0, "    -p PRIMARIES   : ICC profile primaries (8 floats, comma separated). rx,ry,gx,gy,bx,by,wx,wy");
    clLog(NULL, 0, "    -q QUALITY     : Output quality for JPG. JP2 can also use it (see -r below). (default: 90)");
    clLog(NULL, 0, "    -r RATE        : Output rate for JP2. If 0, JP2 codec uses -q value above instead. (default: 150)");
    clLog(NULL, 0, "    -t TONEMAP     : Set tonemapping. auto (default), on, or off");
    clLog(NULL, 0, "    -v             : Verbose mode.");
    clLog(NULL, 0, "    -z x,y,w,h     : Pixels to dump in identify mode. x,y,w,h");
    clLog(NULL, 0, "");
    clLog(NULL, 0, "CPUs Available: %d", clTaskLimit());
    clLog(NULL, 0, "");
    clDumpVersions();
}

int main(int argc, char * argv[])
{
    Args argsStorage;
    Args * args = &argsStorage;
    setDefaults(args);
    if (!parseArgs(args, argc, argv)) {
        return 1;
    }
    if (args->help || (args->action == ACTION_NONE)) {
        printSyntax();
        return 0;
    }
    if (!validateArgs(args)) {
        return 1;
    }
    if (args->verbose)
        dumpArgs(args);

    switch (args->action) {
        case ACTION_CONVERT:
            return actionConvert(args);
            break;
        case ACTION_GENERATE:
            return actionGenerate(args);
            break;
        case ACTION_IDENTIFY:
            return actionIdentify(args);
            break;
        default:
            clLogError("Unimplemented action: %s", actionToString(args->action));
            break;
    }
    return 1;
}
