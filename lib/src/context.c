// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/profile.h"
#include "colorist/task.h"
#include "colorist/transform.h"

#include "lcms2.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Output luminance colorist uses for basic profiles (sRGB, P3, etc)
#define COLORIST_DEFAULT_LUMINANCE 80

#define CL_DEFAULT_QUALITY 90 // ?
#define CL_DEFAULT_RATE 0     // Choosing a value here is dangerous as it is heavily impacted by image size

// ------------------------------------------------------------------------------------------------
// Stock Primaries

typedef struct StockPrimaries
{
    const char * name;
    const char * prettyName;
    clProfilePrimaries primaries;
} StockPrimaries;

static StockPrimaries stockPrimaries[] = {
    { "bt709", "BT.709", { { 0.64f, 0.33f }, { 0.30f, 0.60f }, { 0.15f, 0.06f }, { 0.3127f, 0.3290f } } },
    { "bt2020", "BT.2020", { { 0.708f, 0.292f }, { 0.170f, 0.797f }, { 0.131f, 0.046f }, { 0.3127f, 0.3290f } } },
    { "p3", "P3", { { 0.68f, 0.32f }, { 0.265f, 0.690f }, { 0.150f, 0.060f }, { 0.3127f, 0.3290f } } }
};
static const unsigned int stockPrimariesCount = sizeof(stockPrimaries) / sizeof(stockPrimaries[0]);

// ------------------------------------------------------------------------------------------------
// clAction

clAction clActionFromString(struct clContext * C, const char * str)
{
    COLORIST_UNUSED(C);

    if (!strcmp(str, "identify"))
        return CL_ACTION_IDENTIFY;
    if (!strcmp(str, "id"))
        return CL_ACTION_IDENTIFY;
    if (!strcmp(str, "generate"))
        return CL_ACTION_GENERATE;
    if (!strcmp(str, "gen"))
        return CL_ACTION_GENERATE;
    if (!strcmp(str, "calc"))
        return CL_ACTION_CALC;
    if (!strcmp(str, "convert"))
        return CL_ACTION_CONVERT;
    if (!strcmp(str, "modify"))
        return CL_ACTION_MODIFY;
    return CL_ACTION_ERROR;
}

const char * clActionToString(struct clContext * C, clAction action)
{
    COLORIST_UNUSED(C);

    switch (action) {
        case CL_ACTION_NONE:
            return "--";
        case CL_ACTION_IDENTIFY:
            return "identify";
        case CL_ACTION_GENERATE:
            return "generate";
        case CL_ACTION_CALC:
            return "calc";
        case CL_ACTION_CONVERT:
            return "convert";
        case CL_ACTION_MODIFY:
            return "modify";
        case CL_ACTION_ERROR:
        default:
            break;
    }
    return "unknown";
}

// ------------------------------------------------------------------------------------------------
// clFormat

static char const * clFormatDetectHeader(struct clContext * C, const char * filename)
{
    clRaw raw = CL_RAW_EMPTY;
    if (clRawReadFileHeader(C, &raw, filename, 1024)) {
        for (clFormatRecord * record = C->formats; record != NULL; record = record->next) {
            if (record->format.detectFunc(C, &record->format, &raw)) {
                clRawFree(C, &raw);
                return record->format.name;
            }
        }
    }
    clRawFree(C, &raw);
    return NULL;
}

const char * clFormatDetect(struct clContext * C, const char * filename)
{
    // If either slash is AFTER the last period in the filename, there is no extension
    const char * lastBackSlash = strrchr(filename, '\\');
    const char * lastSlash = strrchr(filename, '/');
    const char * ext = strrchr(filename, '.');
    if ((ext == NULL) || (lastBackSlash && (lastBackSlash > ext)) || (lastSlash && (lastSlash > ext))) {
        ext = clFormatDetectHeader(C, filename);
        if (ext)
            return ext;

        clContextLogError(C, "Unable to guess format");
        return NULL;
    }
    ++ext; // skip past the period

    // Special case: icc profile (this might be bad)
    if (!strcmp(ext, "icc")) {
        return "icc";
    }

    for (clFormatRecord * record = C->formats; record != NULL; record = record->next) {
        int extensionIndex;
        for (extensionIndex = 0; extensionIndex < CL_FORMAT_MAX_EXTENSIONS; ++extensionIndex) {
            if (record->format.extensions[extensionIndex] && !strcmp(record->format.extensions[extensionIndex], ext)) {
                return record->format.name;
            }
        }
    }

    ext = clFormatDetectHeader(C, filename);
    if (ext)
        return ext;

    return NULL;
}

int clFormatMaxDepth(struct clContext * C, const char * formatName)
{
    clFormat * format = clContextFindFormat(C, formatName);
    if (!format) {
        clContextLogError(C, "clFormatMaxDepth() called on unknown format");
        return 8;
    }

    switch (format->depth) {
        case CL_FORMAT_DEPTH_8:
            break;
        case CL_FORMAT_DEPTH_8_OR_10:
            return 10;
        case CL_FORMAT_DEPTH_8_OR_10_OR_12:
            return 12;
        case CL_FORMAT_DEPTH_8_OR_16:
        case CL_FORMAT_DEPTH_8_TO_16:
            return 16;
        case CL_FORMAT_DEPTH_8_OR_16_OR_32:
            return 32;
    }
    return 8;
}

int clFormatBestDepth(struct clContext * C, const char * formatName, int reqDepth)
{
    clFormatDepth formatDepth = CL_FORMAT_DEPTH_8_TO_16; // start with no restrictions
    if (formatName) {
        clFormat * format = clContextFindFormat(C, formatName);
        if (!format) {
            clContextLogError(C, "clFormatBestDepth() called on unknown format");
            return 8;
        }
        formatDepth = format->depth;
    }

    if (reqDepth <= 8) {
        return 8;
    }

    switch (formatDepth) {
        case CL_FORMAT_DEPTH_8:
            break;
        case CL_FORMAT_DEPTH_8_OR_10:
            if (reqDepth == 10)
                return 10;
            break;
        case CL_FORMAT_DEPTH_8_OR_10_OR_12:
            if ((reqDepth > 8) && (reqDepth <= 10))
                return 10;
            if (reqDepth > 10)
                return 12;
            break;
        case CL_FORMAT_DEPTH_8_OR_16:
            if (reqDepth > 8)
                return 16;
            break;
        case CL_FORMAT_DEPTH_8_TO_16:
            if (reqDepth > 16)
                return 16;
            return reqDepth;
        case CL_FORMAT_DEPTH_8_OR_16_OR_32:
            if (reqDepth == 32)
                return 32;
            if (reqDepth > 8)
                return 16;
            break;
    }

    // Everything else gets 8 bit
    return 8;
}

clBool clFormatExists(struct clContext * C, const char * formatName)
{
    return clContextFindFormat(C, formatName) != NULL;
}

// ------------------------------------------------------------------------------------------------
// clTonemap

clBool clTonemapFromString(struct clContext * C, const char * str, clTonemap * outTonemap, clTonemapParams * outParams)
{
    char * buffer = clContextStrdup(C, str);

    for (char * token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        char * equals = strchr(token, '=');
        if (equals == NULL) {
            if (!strcmp(token, "on")) {
                *outTonemap = CL_TONEMAP_ON;
            } else if (!strcmp(token, "yes")) {
                *outTonemap = CL_TONEMAP_ON;
            } else if (!strcmp(token, "enabled")) {
                *outTonemap = CL_TONEMAP_ON;
            } else if (!strcmp(token, "off")) {
                *outTonemap = CL_TONEMAP_OFF;
            } else if (!strcmp(token, "no")) {
                *outTonemap = CL_TONEMAP_OFF;
            } else if (!strcmp(token, "disabled")) {
                *outTonemap = CL_TONEMAP_OFF;
            } else if (!strcmp(token, "auto")) {
                *outTonemap = CL_TONEMAP_AUTO;
            } else if (!strcmp(token, "automatic")) {
                *outTonemap = CL_TONEMAP_AUTO;
            } else {
                clContextLogError(C, "Tonemap: Unexpected option: %s", str);
                return clFalse;
            }
        } else {
            *equals = 0;
            char * valueString = equals + 1;
            char * endptr = NULL;
            float value = (float)strtod(valueString, &endptr);
            if (endptr == valueString) {
                clContextLogError(C, "Tonemap: not a floating point value: %s", valueString);
                return clFalse;
            }

            if (!strcmp(token, "contrast")) {
                outParams->contrast = value;
            } else if (!strcmp(token, "clip")) {
                outParams->clipPoint = value;
            } else if (!strcmp(token, "clippoint")) {
                outParams->clipPoint = value;
            } else if (!strcmp(token, "speed")) {
                outParams->speed = value;
            } else if (!strcmp(token, "power")) {
                outParams->power = value;
            }
        }
    }

    clFree(buffer);
    return clTrue;
}

// ----------------------------------------------------------------------------
// clTonemapParams

void clTonemapParamsSetDefaults(struct clContext * C, clTonemapParams * params)
{
    COLORIST_UNUSED(C);

    params->contrast = 1.0f;
    params->clipPoint = 1.0f;
    params->speed = 1.0f;
    params->power = 1.0f;
}

// ------------------------------------------------------------------------------------------------
// clFilter

clFilter clFilterFromString(struct clContext * C, const char * str)
{
    COLORIST_UNUSED(C);

    if (!strcmp(str, "auto"))
        return CL_FILTER_AUTO;
    if (!strcmp(str, "box"))
        return CL_FILTER_BOX;
    if (!strcmp(str, "triangle"))
        return CL_FILTER_TRIANGLE;
    if (!strcmp(str, "cubic"))
        return CL_FILTER_CUBICBSPLINE;
    if (!strcmp(str, "catmullrom"))
        return CL_FILTER_CATMULLROM;
    if (!strcmp(str, "mitchell"))
        return CL_FILTER_MITCHELL;
    if (!strcmp(str, "nearest"))
        return CL_FILTER_NEAREST;
    return CL_FILTER_INVALID;
}

const char * clFilterToString(struct clContext * C, clFilter filter)
{
    COLORIST_UNUSED(C);

    switch (filter) {
        case CL_FILTER_AUTO:
            return "auto";
        case CL_FILTER_BOX:
            return "box";
        case CL_FILTER_TRIANGLE:
            return "triangle";
        case CL_FILTER_CUBICBSPLINE:
            return "cubic";
        case CL_FILTER_CATMULLROM:
            return "catmullrom";
        case CL_FILTER_MITCHELL:
            return "mitchell";
        case CL_FILTER_NEAREST:
            return "nearest";
        case CL_FILTER_INVALID:
        default:
            break;
    }
    return "invalid";
}

// ------------------------------------------------------------------------------------------------
// clYUVFormat

clYUVFormat clYUVFormatFromString(struct clContext * C, const char * str)
{
    COLORIST_UNUSED(C);

    if (!strcmp(str, "auto"))
        return CL_YUVFORMAT_AUTO;
    if (!strcmp(str, "444"))
        return CL_YUVFORMAT_444;
    if (!strcmp(str, "422"))
        return CL_YUVFORMAT_422;
    if (!strcmp(str, "420"))
        return CL_YUVFORMAT_420;
    if (!strcmp(str, "yv12"))
        return CL_YUVFORMAT_YV12;
    return CL_YUVFORMAT_INVALID;
}

const char * clYUVFormatToString(struct clContext * C, clYUVFormat format)
{
    COLORIST_UNUSED(C);

    switch (format) {
        case CL_YUVFORMAT_AUTO:
            return "auto";
        case CL_YUVFORMAT_444:
            return "444";
        case CL_YUVFORMAT_422:
            return "422";
        case CL_YUVFORMAT_420:
            return "420";
        case CL_YUVFORMAT_YV12:
            return "yv12";
        case CL_YUVFORMAT_INVALID:
        default:
            break;
    }
    return "invalid";
}

clYUVFormat clYUVFormatAutoChoose(struct clContext * C, struct clWriteParams * writeParams)
{
    // This function is a work in progress!

    COLORIST_UNUSED(C);

    if ((writeParams->quality == 0) || (writeParams->quality == 100)) {
        // Lossless gets the "best"
        return CL_YUVFORMAT_444;
    }

    // If you're happy with lossy, let's get lossy!
    return CL_YUVFORMAT_420;
}

// ------------------------------------------------------------------------------------------------
// clContext

static void clConversionParamsSetOutputProfileDefaults(clContext * C, clConversionParams * params)
{
    COLORIST_UNUSED(C);

    params->autoGrade = clFalse;
    params->copyright = NULL;
    params->description = NULL;
    params->curveType = CL_PCT_GAMMA;
    params->frameIndex = 0;
    params->gamma = 0;
    params->luminance = CL_LUMINANCE_SOURCE;
    memset(params->primaries, 0, sizeof(float) * 8);
}

void clConversionParamsSetDefaults(clContext * C, clConversionParams * params)
{
    clConversionParamsSetOutputProfileDefaults(C, params);
    params->bpc = 0;
    params->formatName = NULL;
    params->hald = NULL;
    params->iccOverrideOut = NULL;
    params->rect[0] = 0;
    params->rect[1] = 0;
    params->rect[2] = -1;
    params->rect[3] = -1;
    params->resizeW = 0;
    params->resizeH = 0;
    params->resizeFilter = CL_FILTER_AUTO;
    params->stripTags = NULL;
    params->stats = clFalse;
    params->tonemap = CL_TONEMAP_AUTO;
    params->readCodec = NULL;
    clTonemapParamsSetDefaults(C, &params->tonemapParams);
    params->compositeFilename = NULL;
    clWriteParamsSetDefaults(C, &params->writeParams);
    clBlendParamsSetDefaults(C, &params->compositeParams);
}

void clWriteParamsSetDefaults(struct clContext * C, clWriteParams * writeParams)
{
    COLORIST_UNUSED(C);

    writeParams->quality = CL_DEFAULT_QUALITY;
    writeParams->rate = CL_DEFAULT_RATE;
    writeParams->yuvFormat = CL_YUVFORMAT_AUTO;
    writeParams->writeProfile = clTrue;
    writeParams->quantizerMin = -1;
    writeParams->quantizerMax = -1;
    writeParams->tileRowsLog2 = 0;
    writeParams->tileColsLog2 = 0;
    writeParams->codec = NULL;
}

static void clContextSetDefaultArgs(clContext * C)
{
    C->action = CL_ACTION_NONE;
    clConversionParamsSetDefaults(C, &C->params);
    C->help = clFalse;
    C->iccOverrideIn = NULL;
    C->jobs = clTaskLimit();
    C->verbose = clFalse;
    C->ccmmAllowed = clTrue;
    C->inputFilename = NULL;
    C->outputFilename = NULL;
    C->defaultLuminance = COLORIST_DEFAULT_LUMINANCE;
}

clContext * clContextCreate(clContextSystem * system)
{
    // bootstrap!
    clContextAllocFunc alloc = clContextDefaultAlloc;
    if (system && system->alloc)
        alloc = system->alloc;
    clContext * C = (clContext *)alloc(NULL, sizeof(clContext));
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

    clContextSetDefaultArgs(C);
    clContextRegisterBuiltinFormats(C);
    return C;
}

void clContextDestroy(clContext * C)
{
    clFormatRecord * record = C->formats;
    while (record != NULL) {
        clFormatRecord * freeme = record;
        record = record->next;
        clFree(freeme);
    }
    C->formats = NULL;
    cmsDeleteContext(C->lcms);
    clFree(C);
}

void clContextRegisterFormat(clContext * C, clFormat * format)
{
    clFormatRecord * record = clAllocateStruct(clFormatRecord);
    memcpy(&record->format, format, sizeof(clFormat));
    record->next = NULL;

    if (C->formats) {
        clFormatRecord * prev = C->formats;
        while (prev->next != NULL) {
            prev = prev->next;
        }
        prev->next = record;
    } else {
        C->formats = record;
    }
}

struct clFormat * clContextFindFormat(struct clContext * C, const char * formatName)
{
    clFormatRecord * record = C->formats;
    if (formatName == NULL)
        return NULL;
    for (; record != NULL; record = record->next) {
        if (!strcmp(record->format.name, formatName)) {
            return &record->format;
        }
    }
    return NULL;
}

clBool clContextGetStockPrimaries(struct clContext * C, const char * name, struct clProfilePrimaries * outPrimaries)
{
    COLORIST_UNUSED(C);

    for (unsigned int index = 0; index < stockPrimariesCount; ++index) {
        if (!strcmp(name, stockPrimaries[index].name)) {
            memcpy(outPrimaries, &stockPrimaries[index].primaries, sizeof(clProfilePrimaries));
            return clTrue;
        }
    }
    return clFalse;
}

clBool clContextGetRawStockPrimaries(struct clContext * C, const char * name, float outPrimaries[8])
{
    COLORIST_UNUSED(C);

    for (unsigned int index = 0; index < stockPrimariesCount; ++index) {
        if (!strcmp(name, stockPrimaries[index].name)) {
            memcpy(outPrimaries, &stockPrimaries[index].primaries, sizeof(float) * 8);
            return clTrue;
        }
    }
    return clFalse;
}

const char * clContextFindStockPrimariesPrettyName(struct clContext * C, clProfilePrimaries * primaries)
{
    for (unsigned int index = 0; index < stockPrimariesCount; ++index) {
        if (clProfilePrimariesMatch(C, primaries, &stockPrimaries[index].primaries)) {
            return stockPrimaries[index].prettyName;
        }
    }
    return NULL;
}

// ------------------------------------------------------------------------------------------------
// Memory management

char * clContextStrdup(clContext * C, const char * str)
{
    size_t len = strlen(str);
    char * dup = clAllocate(len + 1);
    memcpy(dup, str, len);
    dup[len] = 0;
    return dup;
}

// ------------------------------------------------------------------------------------------------
// Argument parsing

static clBool parsePrimaries(clContext * C, float primaries[8], const char * arg)
{
    clProfilePrimaries tmpPrimaries;
    if (clContextGetStockPrimaries(C, arg, &tmpPrimaries)) {
        primaries[0] = tmpPrimaries.red[0];
        primaries[1] = tmpPrimaries.red[1];
        primaries[2] = tmpPrimaries.green[0];
        primaries[3] = tmpPrimaries.green[1];
        primaries[4] = tmpPrimaries.blue[0];
        primaries[5] = tmpPrimaries.blue[1];
        primaries[6] = tmpPrimaries.white[0];
        primaries[7] = tmpPrimaries.white[1];
        return clTrue;
    }

    char * buffer = clContextStrdup(C, arg);
    int index = 0;
    for (char * token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
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
    int defaultRect[4] = { 0, 0, 1, 1 }; // the top-left pixel

    int index = 0;
    for (char * token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (index >= 4) {
            clContextLogError(C, "Too many values for rect: (expecting: x,y,w,h)");
            return clFalse;
        }
        rect[index] = atoi(token);
        ++index;
    }
    while (index < 4) {
        rect[index] = defaultRect[index];
        ++index;
    }
    clFree(buffer);
    return clTrue;
}

static clBool parseResize(clContext * C, clConversionParams * params, const char * arg)
{
    static const char * delims = ",x";

    clBool gotWidth = clFalse;
    clBool gotHeight = clFalse;
    char * buffer = clContextStrdup(C, arg);
    for (char * token = strtok(buffer, delims); token != NULL; token = strtok(NULL, delims)) {
        if (isdigit(token[0])) {
            if (!gotWidth) {
                gotWidth = clTrue;
                params->resizeW = atoi(token);
                continue;
            }
            if (!gotHeight) {
                gotHeight = clTrue;
                params->resizeH = atoi(token);
                continue;
            }

            clContextLogError(C, "Too many numerical parameters for --resize");
            return clFalse;
        }

        if (!strcmp(token, "auto")) {
            params->resizeFilter = CL_FILTER_AUTO;
            continue;
        }
        if (!strcmp(token, "bo")) { // Awful hack: Delims include 'x', which truncates box. Allow 'bo' to mean box.
            params->resizeFilter = CL_FILTER_BOX;
            continue;
        }
        if (!strcmp(token, "triangle")) {
            params->resizeFilter = CL_FILTER_TRIANGLE;
            continue;
        }
        if (!strcmp(token, "cubic")) {
            params->resizeFilter = CL_FILTER_CUBICBSPLINE;
            continue;
        }
        if (!strcmp(token, "catmullrom")) {
            params->resizeFilter = CL_FILTER_CATMULLROM;
            continue;
        }
        if (!strcmp(token, "mitchell")) {
            params->resizeFilter = CL_FILTER_MITCHELL;
            continue;
        }
        if (!strcmp(token, "nearest")) {
            params->resizeFilter = CL_FILTER_NEAREST;
            continue;
        }

        clContextLogError(C, "Unrecognized resize filter: %s", token);
        return clFalse;
    }
    clFree(buffer);
    if ((params->resizeW == 0) && (params->resizeH == 0)) {
        clContextLogError(C, "Resize (-r) missing at least one non-zero dimension");
        return clFalse;
    }
    return clTrue;
}

#define NEXTARG()                                                     \
    if (((argIndex + 1) == argc) || (argv[argIndex + 1][0] == '-')) { \
        clContextLogError(C, "%s requires an argument.", arg);        \
        return clFalse;                                               \
    }                                                                 \
    arg = argv[++argIndex]

static clBool validateArgs(clContext * C);

clBool clContextParseArgs(clContext * C, int argc, const char * argv[])
{
    clContextSetDefaultArgs(C); // Reset to all defaults

    int taskLimit = clTaskLimit();

    int argIndex = 1;
    const char * filenames[2] = { NULL, NULL };
    while (argIndex < argc) {
        const char * arg = argv[argIndex];
        if ((arg[0] == '-')) {
            if (!strcmp(arg, "-a") || !strcmp(arg, "--auto") || !strcmp(arg, "--autograde")) {
                C->params.autoGrade = clTrue;
            } else if (!strcmp(arg, "-b") || !strcmp(arg, "--bpc")) {
                NEXTARG();
                C->params.bpc = atoi(arg);
                if (C->params.bpc <= 0) {
                    clContextLogError(C, "Invalid --bpc: %s", arg);
                    return clFalse;
                }
            } else if (!strcmp(arg, "-c") || !strcmp(arg, "--copyright")) {
                NEXTARG();
                C->params.copyright = arg;
            } else if (!strcmp(arg, "-d") || !strcmp(arg, "--description")) {
                NEXTARG();
                C->params.description = arg;
            } else if (!strcmp(arg, "-f") || !strcmp(arg, "--format")) {
                NEXTARG();
                C->params.formatName = arg;
                if (!clFormatExists(C, C->params.formatName)) {
                    clContextLogError(C, "Unknown format: %s", arg);
                    return clFalse;
                }
            } else if (!strcmp(arg, "-g") || !strcmp(arg, "--gamma")) {
                NEXTARG();
                if (!strcmp(arg, "hlg")) {
                    C->params.curveType = CL_PCT_HLG;
                    C->params.gamma = 1.0f;
                } else if (!strcmp(arg, "pq")) {
                    C->params.curveType = CL_PCT_PQ;
                    C->params.gamma = 1.0f;
                } else if (arg[0] == 's') {
                    C->params.curveType = CL_PCT_GAMMA;
                    C->params.gamma = -1.0f; // Use source gamma
                } else {
                    C->params.curveType = CL_PCT_GAMMA;
                    C->params.gamma = (float)strtod(arg, NULL);
                }
            } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
                C->help = clTrue;
            } else if (!strcmp(arg, "--hald")) {
                NEXTARG();
                C->params.hald = arg;
            } else if (!strcmp(arg, "-i") || !strcmp(arg, "--iccin")) {
                NEXTARG();
                C->iccOverrideIn = arg;
            } else if (!strcmp(arg, "-j") || !strcmp(arg, "--jobs")) {
                NEXTARG();
                C->jobs = atoi(arg);
                if ((C->jobs <= 0) || (C->jobs > taskLimit))
                    C->jobs = taskLimit;
            } else if (!strcmp(arg, "--json")) {
                // Allow it to exist on the cmdline, it doesn't adjust any params
            } else if (!strcmp(arg, "-l") || !strcmp(arg, "--luminance")) {
                NEXTARG();
                if (arg[0] == 's') {
                    C->params.luminance = CL_LUMINANCE_SOURCE;
                } else if (arg[0] == 'u') {
                    C->params.luminance = CL_LUMINANCE_UNSPECIFIED;
                } else {
                    C->params.luminance = atoi(arg);
                }
            } else if (!strcmp(arg, "-n") || !strcmp(arg, "--noprofile")) {
                C->params.writeParams.writeProfile = clFalse;
            } else if (!strcmp(arg, "-o") || !strcmp(arg, "--iccout")) {
                NEXTARG();
                C->params.iccOverrideOut = arg;
            } else if (!strcmp(arg, "-p") || !strcmp(arg, "--primaries")) {
                NEXTARG();
                if (!parsePrimaries(C, C->params.primaries, arg))
                    return clFalse;
            } else if (!strcmp(arg, "-q") || !strcmp(arg, "--quality")) {
                NEXTARG();
                C->params.writeParams.quality = atoi(arg);
            } else if (!strcmp(arg, "--resize")) {
                NEXTARG();
                if (!parseResize(C, &C->params, arg))
                    return clFalse;
            } else if (!strcmp(arg, "-s") || !strcmp(arg, "--striptags")) {
                NEXTARG();
                C->params.stripTags = arg;
            } else if (!strcmp(arg, "--stats")) {
                C->params.stats = clTrue;
            } else if (!strcmp(arg, "-t") || !strcmp(arg, "--tonemap")) {
                NEXTARG();
                if (!clTonemapFromString(C, arg, &C->params.tonemap, &C->params.tonemapParams)) {
                    return clFalse;
                }
            } else if (!strcmp(arg, "--composite")) {
                NEXTARG();
                C->params.compositeFilename = arg;
            } else if (!strcmp(arg, "--composite-gamma")) {
                NEXTARG();
                C->params.compositeParams.gamma = (float)atof(arg);
                if (C->params.compositeParams.gamma <= 0.0f) {
                    clContextLogError(C, "Invalid composite gamma: %s", arg);
                    return clFalse;
                }
            } else if (!strcmp(arg, "--composite-tonemap")) {
                NEXTARG();
                if (!clTonemapFromString(C, arg, &C->params.compositeParams.cmpTonemap, &C->params.compositeParams.cmpParams)) {
                    return clFalse;
                }
            } else if (!strcmp(arg, "--composite-premultiplied")) {
                C->params.compositeParams.premultiplied = clTrue;
            } else if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose")) {
                C->verbose = clTrue;
            } else if (!strcmp(arg, "--yuv")) {
                NEXTARG();
                C->params.writeParams.yuvFormat = clYUVFormatFromString(C, arg);
                if (C->params.writeParams.yuvFormat == CL_YUVFORMAT_INVALID) {
                    clContextLogError(C, "Unknown YUV Format: %s", arg);
                    return clFalse;
                }
            } else if (!strcmp(arg, "--cmm") || !strcmp(arg, "--cms")) {
                NEXTARG();
                if (!strcmp(arg, "auto") || !strcmp(arg, "colorist") || !strcmp(arg, "ccmm")) {
                    C->ccmmAllowed = clTrue;
                } else if (!strcmp(arg, "lcms") || !strcmp(arg, "littlecms")) {
                    C->ccmmAllowed = clFalse;
                } else {
                    clContextLogError(C, "Unknown CMM: %s", arg);
                    return clFalse;
                }
            } else if (!strcmp(arg, "--deflum")) {
                NEXTARG();
                C->defaultLuminance = atoi(arg);
                if (C->defaultLuminance <= 0) {
                    clContextLogError(C, "Invalid default luminance: %s", arg);
                    return clFalse;
                }
            } else if (!strcmp(arg, "--frameindex")) {
                NEXTARG();
                C->params.frameIndex = (uint32_t)atoi(arg);
            } else if (!strcmp(arg, "--hlglum")) {
                NEXTARG();
                int hlgLum = atoi(arg);
                if (hlgLum <= 0) {
                    clContextLogError(C, "Invalid HLG luminance: %s", arg);
                    return clFalse;
                }
                C->defaultLuminance = clTransformCalcDefaultLuminanceFromHLG(hlgLum);
                clContextLog(C, "hlg", 0, "Choosing %d nits as default luminance based on max HLG luminance of %d nits", C->defaultLuminance, hlgLum);
                if (hlgLum <= 1) {
                    clContextLogError(C, "Invalid HLG luminance: %s", arg);
                    return clFalse;
                }
            } else if (!strcmp(arg, "-z") || !strcmp(arg, "--rect") || !strcmp(arg, "--crop")) {
                NEXTARG();
                if (!parseRect(C, C->params.rect, arg))
                    return clFalse;
            } else if (!strcmp(arg, "--quantizer")) {
                NEXTARG();
                char tmpBuffer[16]; // the biggest legal string is "63,63", so I don't mind truncation here
                strncpy(tmpBuffer, arg, 15);
                tmpBuffer[15] = 0;
                char * comma = strchr(tmpBuffer, ',');
                if (comma) {
                    *comma = 0;
                    ++comma;
                    C->params.writeParams.quantizerMin = atoi(tmpBuffer);
                    C->params.writeParams.quantizerMax = atoi(comma);
                } else {
                    int quantizerBoth = atoi(tmpBuffer);
                    C->params.writeParams.quantizerMin = quantizerBoth;
                    C->params.writeParams.quantizerMax = quantizerBoth;
                }
                C->params.writeParams.quantizerMin = CL_CLAMP(C->params.writeParams.quantizerMin, 0, 63);
                C->params.writeParams.quantizerMax = CL_CLAMP(C->params.writeParams.quantizerMax, 0, 63);
            } else if (!strcmp(arg, "--tiling")) {
                NEXTARG();
                char tmpBuffer[16]; // the biggest legal string is "6,6", so I don't mind truncation here
                strncpy(tmpBuffer, arg, 15);
                tmpBuffer[15] = 0;
                char * comma = strchr(tmpBuffer, ',');
                if (comma) {
                    *comma = 0;
                    ++comma;
                    C->params.writeParams.tileRowsLog2 = atoi(tmpBuffer);
                    C->params.writeParams.tileColsLog2 = atoi(comma);
                } else {
                    int tileBoth = atoi(tmpBuffer);
                    C->params.writeParams.tileRowsLog2 = tileBoth;
                    C->params.writeParams.tileColsLog2 = tileBoth;
                }
                C->params.writeParams.tileRowsLog2 = CL_CLAMP(C->params.writeParams.tileRowsLog2, 0, 6);
                C->params.writeParams.tileColsLog2 = CL_CLAMP(C->params.writeParams.tileColsLog2, 0, 6);
            } else if (!strcmp(arg, "--codec")) {
                NEXTARG();
                char tmpBuffer[64];
                strncpy(tmpBuffer, arg, 63);
                tmpBuffer[63] = 0;
                char * comma = strchr(tmpBuffer, ',');
                if (comma) {
                    *comma = 0;
                    ++comma;
                    C->params.readCodec = clContextStrdup(C, tmpBuffer);
                    C->params.writeParams.codec = clContextStrdup(C, comma);
                } else {
                    C->params.readCodec = clContextStrdup(C, tmpBuffer);
                    C->params.writeParams.codec = clContextStrdup(C, tmpBuffer);
                }
            } else if (!strcmp(arg, "-r") || !strcmp(arg, "--rate")) {
                NEXTARG();
                C->params.writeParams.rate = atoi(arg);
            } else {
                clContextLogError(C, "unknown parameter: %s", arg);
                return clFalse;
            }
        } else {
            if (C->action == CL_ACTION_NONE) {
                C->action = clActionFromString(C, arg);
                if (C->action == CL_ACTION_ERROR) {
                    clContextLogError(C, "unknown action '%s', expecting convert, identify, or generate", arg);
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

        case CL_ACTION_CALC:
            if (filenames[0]) {
                C->inputFilename = filenames[0];
            } else {
                clContextLogError(C, "calc requires an input string.");
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

        case CL_ACTION_MODIFY:
            C->inputFilename = filenames[0];
            if (!C->inputFilename) {
                clContextLogError(C, "modify requires an input filename.");
                return clFalse;
            }
            C->outputFilename = filenames[1];
            if (!C->outputFilename) {
                clContextLogError(C, "modify requires an output filename.");
                return clFalse;
            }
            break;

        case CL_ACTION_ERROR:
            return clFalse;

        case CL_ACTION_NONE:
            break;
    }
    return validateArgs(C);
}

static clBool validateArgs(clContext * C)
{
    if (C->params.autoGrade && (C->params.gamma != 0.0f) && (C->params.luminance != 0)) {
        clContextLog(C, "syntax", 0, "WARNING: auto color grading mode (-a) is useless with both -g and -l specified, disabling auto color grading");
        C->params.autoGrade = clFalse;
    }
    if (C->params.iccOverrideOut) {
        clContextLog(C, "syntax", 0, "-o in use, disabling all other output profile options");
        clConversionParamsSetOutputProfileDefaults(C, &C->params);
    }
    return clTrue;
}

void clContextPrintSyntax(clContext * C)
{
    clFormatRecord * record = C->formats;
    char formatLine[1024]; // TODO: protect this size better
    strcpy(formatLine, "    -f,--format FORMAT       : Output format. auto (default)");
    for (; record != NULL; record = record->next) {
        strcat(formatLine, ", ");
        strcat(formatLine, record->format.name);
    }

    clContextLog(C, NULL, 0, "Syntax: colorist convert  [input]        [output]       [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist identify [input]                       [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist generate                [output.icc]   [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist generate [image string] [output image] [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist modify   [input.icc]    [output.icc]   [OPTIONS]");
    clContextLog(C, NULL, 0, "        colorist calc     [image string]                [OPTIONS]");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Basic Options:");
    clContextLog(C, NULL, 0, "    -h,--help                : Display this help");
    clContextLog(C, NULL, 0, "    -j,--jobs JOBS           : Number of jobs to use when working. 0 for as many as possible (default)");
    clContextLog(C, NULL, 0, "    -v,--verbose             : Verbose mode.");
    clContextLog(C, NULL, 0, "    --cmm WHICH,--cms WHICH  : Choose Color Management Module/System: auto (default), lcms, colorist (built-in, uses when possible)");
    clContextLog(C,
                 NULL,
                 0,
                 "    --deflum LUMINANCE       : Choose the default/fallback luminance value in nits when unspecified (default: %d)",
                 COLORIST_DEFAULT_LUMINANCE);
    clContextLog(C, NULL, 0, "    --hlglum LUMINANCE       : Alternative to --deflum, hlglum chooses an appropriate diffuse white for --deflum based on peak HLG lum.");
    clContextLog(C, NULL, 0, "                               (--hlglum and --deflum are mutually exclusive as they are two ways to set the same value.)");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Input Options:");
    clContextLog(C, NULL, 0, "    -i,--iccin file.icc      : Override source ICC profile. default is to use embedded profile (if any), or sRGB@deflum");
    clContextLog(C, NULL, 0, "    --frameindex INDEX       : Choose the source frame from an image sequence (AVIF only, defaults to frame 0)");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Output Profile Options:");
    clContextLog(C, NULL, 0, "    -o,--iccout file.icc     : Override destination ICC profile. Disables all other output profile options");
    clContextLog(C, NULL, 0, "    -a,--autograde           : Enable automatic color grading of max luminance and gamma (disabled by default)");
    clContextLog(C, NULL, 0, "    -c,--copyright COPYRIGHT : ICC profile copyright string.");
    clContextLog(C, NULL, 0, "    -d,--description DESC    : ICC profile description.");
    clContextLog(C, NULL, 0, "    -g,--gamma GAMMA         : Output gamma (transfer func). 0 for auto (default), \"pq\" for PQ, \"hlg\" for HLG, or \"source\" to force source gamma");
    clContextLog(C, NULL, 0, "    -l,--luminance LUMINANCE : ICC profile max luminance, in nits. \"source\" to match source lum (default), or \"unspecified\" not specify");
    clContextLog(C, NULL, 0, "    -p,--primaries PRIMARIES : Color primaries. Use builtin (bt709, bt2020, p3) or in the form: rx,ry,gx,gy,bx,by,wx,wy");
    clContextLog(C, NULL, 0, "    -n,--noprofile           : Do not write the converted image's profile to the output file. (all profile options still impact image conversion)");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Output Format Options:");
    clContextLog(C, NULL, 0, "    -b,--bpc BPC             : Output bits-per-channel. 8 - 16, or 0 for auto (default)");
    clContextLog(C, NULL, 0, formatLine);
    clContextLog(C, NULL, 0, "    -q,--quality QUALITY     : Output quality for supported output formats. (default: 90)");
    clContextLog(C, NULL, 0, "    -r,--rate RATE           : Output rate for for supported output formats. If 0, codec uses -q value above instead. (default: 0)");
    clContextLog(C, NULL, 0, "    -t,--tonemap TM          : Set tonemapping. auto (default), on, or off. Tune with optional comma separated vals: contrast=1.0,clip=1.0,speed=1.0,power=1.0");
    clContextLog(C, NULL, 0, "    --yuv YUVFORMAT          : Choose yuv output format for supported formats. auto (default), 444, 422, 420, yv12");
    clContextLog(C, NULL, 0, "    --quantizer MIN,MAX      : Choose min and max quantizer values directly instead of using -q (AVIF only, 0-63 range, 0,0 is lossless)");
    clContextLog(C, NULL, 0, "    --tiling ROWS,COLS       : Enable tiling when encoding (AVIF only, 0-6 range, log2 based. Enables 2^ROWS rows and/or 2^COLS cols)");
    clContextLog(C, NULL, 0, "    --codec READ,WRITE       : Specify which internal codec to be used when decoding (AVIF only, auto,auto is default, see libavif version below for choices)");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Convert Options:");
    clContextLog(C, NULL, 0, "    --resize w,h,filter      : Resize dst image to WxH. Use optional filter (auto (default), box, triangle, cubic, catmullrom, mitchell, nearest)");
    clContextLog(C, NULL, 0, "    -z,--rect,--crop x,y,w,h : Crop source image to rect (before conversion). x,y,w,h");
    clContextLog(C, NULL, 0, "    --composite FILENAME     : Composite FILENAME on top of input. Must be identical dimensions to input.");
    clContextLog(C, NULL, 0, "    --composite-gamma GAMMA  : When compositing, perform sourceover blend using this gamma (default: 2.2)");
    clContextLog(C, NULL, 0, "    --composite-premultiplied: When compositing, assume composite image's alpha is premultiplied (default: false)");
    clContextLog(C, NULL, 0, "    --composite-tonemap TM   : When compositing, determines if composite image is tonemapped before blend. auto (default), on, or off");
    clContextLog(C, NULL, 0, "    --hald FILENAME          : Image containing valid Hald CLUT to be used after color conversion");
    clContextLog(C, NULL, 0, "    --stats                  : Enable post-conversion stats (MSE, PSNR, etc)");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Identify / Calc Options:");
    clContextLog(C, NULL, 0, "    -z,--rect x,y,w,h        : Pixels to dump. x,y,w,h");
    clContextLog(C, NULL, 0, "    --json                   : Output valid JSON description instead of standard log output");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "Modify Options:");
    clContextLog(C, NULL, 0, "    -s,--striptags TAG,...   : Strips ICC tags from profile");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "See image string examples here: https://joedrago.github.io/colorist/docs/Usage.html");
    clContextLog(C, NULL, 0, "");
    clContextLog(C, NULL, 0, "CPUs Available: %d", clTaskLimit());
    clContextLog(C, NULL, 0, "");
    clContextPrintVersions(C);
}
