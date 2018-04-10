// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include <stdio.h>
#include <string.h>

static void setDefaults(Args * args)
{
    args->action = ACTION_NONE;
    args->bpp = 0;
    args->copyright = NULL;
    args->description = NULL;
    args->format = FORMAT_AUTO;
    args->gamma = 0;
    args->help = clFalse;
    args->luminance = 0;
    memset(args->primaries, 0, sizeof(float) * 8);
    args->verbose = clFalse;
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
    if (!strcmp(str, "png"))  return FORMAT_PNG;
    if (!strcmp(str, "jpg"))  return FORMAT_JPG;
    if (!strcmp(str, "jp2"))  return FORMAT_JP2;
    if (!strcmp(str, "icc"))  return FORMAT_ICC;
    return FORMAT_ERROR;
}

const char * formatToString(Format format)
{
    switch (format) {
        case FORMAT_AUTO: return "Auto";
        case FORMAT_PNG:  return "PNG";
        case FORMAT_JPG:  return "JPG";
        case FORMAT_JP2:  return "JP2";
        case FORMAT_ICC:  return "ICC";
        case FORMAT_ERROR:
        default:
            break;
    }
    return "Unknown";
}

Format detectFormat(const char * filename)
{
    const char * dot = strrchr(filename, '.');
    if (dot == NULL) {
        fprintf(stderr, "ERROR: Unable to guess format\n");
        return FORMAT_ERROR;
    }
    if (!strcmp(dot + 1, "png")) return FORMAT_PNG;
    if (!strcmp(dot + 1, "jpg")) return FORMAT_JPG;
    if (!strcmp(dot + 1, "jp2")) return FORMAT_JP2;
    if (!strcmp(dot + 1, "icc")) return FORMAT_ICC;
    fprintf(stderr, "ERROR: Unknown file extension '%s'\n", dot + 1);
    return FORMAT_ERROR;
}

#define NEXTARG()                                                      \
    if (((argIndex + 1) == argc) || (argv[argIndex + 1][0] == '-')) {  \
        fprintf(stderr, "ERROR: -%c requires an argument.\n", arg[1]); \
        return clFalse;                                                \
    }                                                                  \
    arg = argv[++argIndex]

static clBool parsePrimaries(float primaries[8], const char * arg)
{
    char * buffer = strdup(arg);
    char * token;
    int priIndex = 0;
    for (token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (priIndex >= 8) {
            fprintf(stderr, "ERROR: Too many primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)\n");
            return clFalse;
        }
        primaries[priIndex] = strtod(token, NULL);
        ++priIndex;
    }
    if (priIndex < 8) {
        fprintf(stderr, "ERROR: Too few primaries: (expecting: rx,ry,gx,gy,bx,by,wx,wy)\n");
        return clFalse;
    }
    return clTrue;
}

static clBool parseArgs(Args * args, int argc, char * argv[])
{
    int argIndex = 1;
    const char * filenames[2] = { NULL, NULL };
    while (argIndex < argc) {
        const char * arg = argv[argIndex];
        if ((arg[0] == '-')) {
            switch (arg[1]) {
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
                    args->gamma = strtod(arg, NULL);
                    break;
                case 'h':
                    args->help = clTrue;
                    break;
                case 'l':
                    NEXTARG();
                    args->luminance = atoi(arg);
                    break;
                case 'p':
                    NEXTARG();
                    if (!parsePrimaries(args->primaries, arg))
                        return clFalse;
                    break;
                case 'v':
                    args->verbose = clTrue;
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
        printf("ERROR: Unknown bpp: %d\n", args->bpp);
        valid = clFalse;
    }
    if ((args->gamma < 0.0f)) {
        printf("ERROR: gamma too small: %g\n", args->gamma);
        valid = clFalse;
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
    if (args->gamma > 0.0f)
        printf(" * gamma      : %g\n", args->gamma);
    else
        printf(" * gamma      : auto\n");
    printf(" * help       : %s\n", args->help ? "enabled" : "disabled");
    if (args->luminance)
        printf(" * luminance  : %d\n", args->luminance);
    else
        printf(" * luminance  : auto\n");
    if (args->primaries[0] > 0.0f)
        printf(" * primaries  : r:(%.4g,%.4g) g:(%.4g,%.4g) b:(%.4g,%.4g) w:(%.4g,%.4g)\n",
            args->primaries[0], args->primaries[1],
            args->primaries[2], args->primaries[3],
            args->primaries[4], args->primaries[5],
            args->primaries[6], args->primaries[7]);
    else
        printf(" * primaries  : auto\n");
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
    printf("    -b BPP         : Output bits-per-pixel. 8, 16, or 0 for auto (default)\n");
    printf("    -c COPYRIGHT   : ICC profile copyright string.\n");
    printf("    -d DESCRIPTION : ICC profile description.\n");
    printf("    -f FORMAT      : Output format. auto (default), jpg, png, jp2\n");
    printf("    -g GAMMA       : Output gamma. 0 for auto (default)\n");
    printf("    -h             : Display this help\n");
    printf("    -l LUMINANCE   : ICC profile max luminance. 0 for auto (default)\n");
    printf("    -p PRIMARIES   : ICC profile primaries (8 floats, comma separated). rx,ry,gx,gy,bx,by,wx,wy\n");
    printf("    -v             : Verbose mode.\n");
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
        case ACTION_IDENTIFY:
            return actionIdentify(args);
            break;
        default:
            fprintf(stderr, "ERROR: Unimplemented action: %s\n", actionToString(args->action));
            break;
    }
    return 1;
}

// {
//     clImage * image = clImageCreate(2, 2, 8, NULL);
//     clImageSetPixel(image, 0, 0, 255, 128, 238, 0);
//     clImageDebugDump(image);
//     clImageChangeDepth(image, 16);
//     clImageDebugDump(image);
//     clImageChangeDepth(image, 8);
//     clImageDebugDump(image);
//     clImageDestroy(image);
// }
// {
//     clImage * image;
//     clProfile * profile;
//     profile = clProfileParse((const uint8_t *)"foo", 3);
//     clProfileDestroy(profile);
// }
// {
//     cmsHPROFILE srcProfile = cmsOpenProfileFromFile("f:\\work\\hdr10.icc", "r");
//     cmsHPROFILE dstProfile = cmsOpenProfileFromFile("f:\\work\\srgb.icc", "r");
//     cmsHPROFILE dstProfileLin = createDstLinearProfile(dstProfile);
//     cmsHTRANSFORM toLinear = cmsCreateTransform(srcProfile, TYPE_RGBA_8, dstProfileLin, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);
//     cmsHTRANSFORM fromLinear = cmsCreateTransform(dstProfileLin, TYPE_RGBA_FLT, dstProfile, TYPE_RGBA_8, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);
//     uint8_t srcPixel[4] = { 224, 145, 72, 255 };
//     float dstPixelLin[4];
//     uint8_t dstPixel[4];
//     cmsDoTransform(toLinear, srcPixel, dstPixelLin, 1);
//     cmsDoTransform(fromLinear, dstPixelLin, dstPixel, 1);
//     cmsCloseProfile(srcProfile);
//     cmsCloseProfile(dstProfile);
//     cmsCloseProfile(toLinear);
// }

// cmsHPROFILE createDstLinearProfile(cmsHPROFILE srcProfile)
// {
//     cmsHPROFILE outProfile;
//     cmsCIEXYZ * dstRXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigRedColorantTag);
//     cmsCIEXYZ * dstGXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigGreenColorantTag);
//     cmsCIEXYZ * dstBXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigBlueColorantTag);
//     cmsCIEXYZ * dstWXYZ = (cmsCIEXYZ *)cmsReadTag(srcProfile, cmsSigMediaWhitePointTag);
//     cmsToneCurve * gamma1 = cmsBuildGamma(NULL, 1.0);
//     cmsToneCurve * curves[3];
//     cmsCIExyYTRIPLE dstPrimaries;
//     cmsCIExyY dstWhitePoint;
//     cmsXYZ2xyY(&dstPrimaries.Red, dstRXYZ);
//     cmsXYZ2xyY(&dstPrimaries.Green, dstGXYZ);
//     cmsXYZ2xyY(&dstPrimaries.Blue, dstBXYZ);
//     cmsXYZ2xyY(&dstWhitePoint, dstWXYZ);
//     curves[0] = gamma1;
//     curves[1] = gamma1;
//     curves[2] = gamma1;
//     outProfile = cmsCreateRGBProfile(&dstWhitePoint, &dstPrimaries, curves);
//     cmsFreeToneCurve(gamma1);
//     return outProfile;
// }
