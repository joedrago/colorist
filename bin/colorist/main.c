// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include <stdio.h>
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
    args->luminance = 0;
    memset(args->primaries, 0, sizeof(float) * 8);
    args->quality = 60; // ?
    args->rate = 200;   // ?
    args->verbose = clFalse;
    args->rect[0] = 0;
    args->rect[1] = 0;
    args->rect[2] = 3;
    args->rect[3] = 3;
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
        fprintf(stderr, "ERROR: Unable to guess format\n");
        return FORMAT_ERROR;
    }
    ++ext; // skip past the period
    if (!strcmp(ext, "icc")) return FORMAT_ICC;
    if (!strcmp(ext, "jp2")) return FORMAT_JP2;
    if (!strcmp(ext, "jpg")) return FORMAT_JPG;
    if (!strcmp(ext, "png")) return FORMAT_PNG;
    fprintf(stderr, "ERROR: Unknown file extension '%s'\n", ext);
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
            fprintf(stderr, "ERROR: Too many primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)\n");
            return clFalse;
        }
        primaries[index] = (float)strtod(token, NULL);
        ++index;
    }
    if (index < 8) {
        fprintf(stderr, "ERROR: Too few primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)\n");
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
            fprintf(stderr, "ERROR: Too many values for rect: (expecting: x,y,w,h)\n");
            return clFalse;
        }
        rect[index] = atoi(token);
        ++index;
    }
    if (index < 4) {
        fprintf(stderr, "ERROR: Too few values for rect: (expecting: x,y,w,h)\n");
        return clFalse;
    }
    return clTrue;
}

#define NEXTARG()                                                      \
    if (((argIndex + 1) == argc) || (argv[argIndex + 1][0] == '-')) {  \
        fprintf(stderr, "ERROR: -%c requires an argument.\n", arg[1]); \
        return clFalse;                                                \
    }                                                                  \
    arg = argv[++argIndex]

static clBool parseArgs(Args * args, int argc, char * argv[])
{
    int argIndex = 1;
    const char * filenames[2] = { NULL, NULL };
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
                        printf("ERROR: Unknown format: %s\n", arg);
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
                    fprintf(stderr, "ERROR: unknown action '%s', expecting identify or convert\n", arg);
                    return clFalse;
                }
            } else if (filenames[0] == NULL) {
                filenames[0] = arg;
            } else if (filenames[1] == NULL) {
                filenames[1] = arg;
            } else {
                fprintf(stderr, "ERROR: Too many positional arguments.\n");
                return clFalse;
            }
        }
        ++argIndex;
    }

    switch (args->action) {
        case ACTION_IDENTIFY:
            args->inputFilename = filenames[0];
            if (!args->inputFilename) {
                printf("ERROR: identify requires an input filename.\n");
                return clFalse;
            }
            if (filenames[1]) {
                printf("ERROR: identify does not accept an output filename.\n");
                return clFalse;
            }
            break;

        case ACTION_GENERATE:
            args->outputFilename = filenames[0];
            if (!args->outputFilename) {
                printf("ERROR: generate requires an output filename.\n");
                return clFalse;
            }
            if (filenames[1]) {
                printf("ERROR: generate does not accept both an input and output filename.\n");
                return clFalse;
            }
            break;

        case ACTION_CONVERT:
            args->inputFilename = filenames[0];
            if (!args->inputFilename) {
                printf("ERROR: convert requires an input filename.\n");
                return clFalse;
            }
            args->outputFilename = filenames[1];
            if (!args->outputFilename) {
                printf("ERROR: convert requires an output filename.\n");
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
        fprintf(stderr, "ERROR: Unknown bpp: %d\n", args->bpp);
        valid = clFalse;
    }
    if (args->autoGrade && (args->gamma != 0.0f) && (args->luminance != 0)) {
        fprintf(stderr, "WARNING: auto color grading mode (-a) is useless with both -g and -l specified, disabling auto color grading\n");
        args->autoGrade = clFalse;
    }
    return valid;
}

static void dumpArgs(Args * args)
{
    printf("Args:\n");
    printf(" * Action     : %s\n", actionToString(args->action));
    if (args->bpp)
        printf(" * bpp        : %d\n", args->bpp);
    else
        printf(" * bpp        : auto\n");
    printf(" * copyright  : %s\n", args->copyright ? args->copyright : "--");
    printf(" * description: %s\n", args->description ? args->description : "--");
    printf(" * format     : %s\n", formatToString(args->format));
    if (args->gamma < 0.0f) {
        printf(" * gamma      : source gamma (forced)\n");
    } else if (args->gamma > 0.0f)
        printf(" * gamma      : %g\n", args->gamma);
    else
        printf(" * gamma      : auto\n");
    printf(" * help       : %s\n", args->help ? "enabled" : "disabled");
    if (args->luminance < 0) {
        printf(" * luminance  : source luminance (forced)\n");
    } else if (args->luminance) {
        printf(" * luminance  : %d\n", args->luminance);
    } else {
        printf(" * luminance  : auto\n");
    }
    if (args->primaries[0] > 0.0f)
        printf(" * primaries  : r:(%.4g,%.4g) g:(%.4g,%.4g) b:(%.4g,%.4g) w:(%.4g,%.4g)\n",
            args->primaries[0], args->primaries[1],
            args->primaries[2], args->primaries[3],
            args->primaries[4], args->primaries[5],
            args->primaries[6], args->primaries[7]);
    else
        printf(" * primaries  : auto\n");
    printf(" * rect       : (%d,%d) %dx%d\n", args->rect[0], args->rect[1], args->rect[2], args->rect[3]);
    printf(" * verbose    : %s\n", args->verbose ? "enabled" : "disabled");
    printf(" * input      : %s\n", args->inputFilename ? args->inputFilename : "--");
    printf(" * output     : %s\n", args->outputFilename ? args->outputFilename : "--");
}

static void printSyntax()
{
    printf("         colorist convert  [input] [output] [OPTIONS]\n");
    printf("Syntax : colorist identify [input]          [OPTIONS]\n");
    printf("         colorist generate         [output] [OPTIONS]\n");
    printf("Options:\n");
    printf("    -a             : Enable automatic color grading of max luminance and gamma (disabled by default)\n");
    printf("    -b BPP         : Output bits-per-pixel. 8, 16, or 0 for auto (default)\n");
    printf("    -c COPYRIGHT   : ICC profile copyright string.\n");
    printf("    -d DESCRIPTION : ICC profile description.\n");
    printf("    -f FORMAT      : Output format. auto (default), icc, jp2, jpg, png\n");
    printf("    -g GAMMA       : Output gamma. 0 for auto (default), or \"source\" to force source gamma\n");
    printf("    -h             : Display this help\n");
    printf("    -l LUMINANCE   : ICC profile max luminance. 0 for auto (default), or \"source\" to force source luminance\n");
    printf("    -p PRIMARIES   : ICC profile primaries (8 floats, comma separated). rx,ry,gx,gy,bx,by,wx,wy\n");
    printf("    -q QUALITY     : Output quality for JPG/JP2\n");
    printf("    -r RATE        : Output rate for JP2\n");
    printf("    -t TONEMAP     : Set tonemapping. auto (default), on, or off\n");
    printf("    -v             : Verbose mode.\n");
    printf("    -z x,y,w,h     : Pixels to dump in identify mode. x,y,w,h\n");
    printf("\n");
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
            fprintf(stderr, "ERROR: Unimplemented action: %s\n", actionToString(args->action));
            break;
    }
    return 1;
}
