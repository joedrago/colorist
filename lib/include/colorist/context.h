// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_CONTEXT_H
#define COLORIST_CONTEXT_H

#include "colorist/types.h"

// for cmsContext
#include "lcms2.h"

// for va_list
#include <stdarg.h>

struct clContext;
struct clProfilePrimaries;

typedef enum clAction
{
    CL_ACTION_NONE = 0,
    CL_ACTION_CONVERT,
    CL_ACTION_GENERATE,
    CL_ACTION_IDENTIFY,
    CL_ACTION_MODIFY,
    CL_ACTION_REPORT,

    CL_ACTION_ERROR
} clAction;

clAction clActionFromString(struct clContext * C, const char * str);
const char * clActionToString(struct clContext * C, clAction action);

typedef enum clFormat
{
    CL_FORMAT_AUTO = 0,
    CL_FORMAT_ICC,
    CL_FORMAT_J2K,
    CL_FORMAT_JP2,
    CL_FORMAT_JPG,
    CL_FORMAT_PNG,
    CL_FORMAT_TIFF,
    CL_FORMAT_WEBP,

    CL_FORMAT_ERROR
} clFormat;

clFormat clFormatFromString(struct clContext * C, const char * str);
const char * clFormatToString(struct clContext * C, clFormat format);
clFormat clFormatDetect(struct clContext * C, const char * filename);
int clFormatMaxDepth(struct clContext * C, clFormat format);

typedef enum clTonemap
{
    CL_TONEMAP_AUTO = 0,
    CL_TONEMAP_ON,
    CL_TONEMAP_OFF
} clTonemap;

clTonemap clTonemapFromString(struct clContext * C, const char * str);
const char * clTonemapToString(struct clContext * C, clTonemap tonemap);

// Filter enumeration and comments taken directly from stb_image_resize
// (with minor tweaks like DEFAULT -> AUTO, addition of NEAREST)
typedef enum clFilter
{
    CL_FILTER_AUTO         = 0, // Choose best based on upsampling or downsampling
    CL_FILTER_BOX          = 1, // A trapezoid w/1-pixel wide ramps, same result as box for integer scale ratios
    CL_FILTER_TRIANGLE     = 2, // On upsampling, produces same results as bilinear texture filtering
    CL_FILTER_CUBICBSPLINE = 3, // The cubic b-spline (aka Mitchell-Netrevalli with B=1,C=0), gaussian-esque
    CL_FILTER_CATMULLROM   = 4, // An interpolating cubic spline
    CL_FILTER_MITCHELL     = 5, // Mitchell-Netrevalli filter with B=1/3, C=1/3
    CL_FILTER_NEAREST      = 6, // Doesn't use stb_image_resize, just does an obvious nearest neighbor

    CL_FILTER_INVALID      = -1
} clFilter;

clFilter clFilterFromString(struct clContext * C, const char * str);
const char * clFilterToString(struct clContext * C, clFilter filter);

typedef void *(* clContextAllocFunc)(struct clContext * C, int bytes); // C will be NULL when allocating the clContext itself
typedef void (* clContextFreeFunc)(struct clContext * C, void * ptr);
typedef void (* clContextLogFunc)(struct clContext * C, const char * section, int indent, const char * format, va_list args);
typedef void (* clContextLogErrorFunc)(struct clContext * C, const char * format, va_list args);

// Internal defaults for clContextSystem, use clContextLog*() / clAllocate / clFree below
void * clContextDefaultAlloc(struct clContext * C, int bytes);
void clContextDefaultFree(struct clContext * C, void * ptr);
void clContextDefaultLog(struct clContext * C, const char * section, int indent, const char * format, va_list args);
void clContextDefaultLogError(struct clContext * C, const char * format, va_list args);

typedef struct clContextSystem
{
    clContextAllocFunc alloc;
    clContextFreeFunc free;
    clContextLogFunc log;
    clContextLogErrorFunc error;
} clContextSystem;

typedef struct clConversionParams
{
    clBool autoGrade;            // -a
    int bpp;                     // -b
    const char * copyright;      // -c
    const char * description;    // -d
    clFormat format;             // -f
    float gamma;                 // -g
    int jobs;                    // -j
    int luminance;               // -l
    const char * iccOverrideOut; // -o
    float primaries[8];          // -p
    int quality;                 // -q
    int resizeW;                 // -r
    int resizeH;                 // -r
    clFilter resizeFilter;       // -r
    const char * stripTags;      // -s
    clTonemap tonemap;           // -t
    int rect[4];                 // -z
    int jp2rate;                 // -2
} clConversionParams;

void clConversionParamsSetDefaults(struct clContext * C, clConversionParams * params);

typedef struct clContext
{
    clContextSystem system;

    cmsContext lcms;

    clAction action;
    clConversionParams params;   // see above
    clBool help;                 // -h
    const char * iccOverrideIn;  // -i
    clBool verbose;              // -v
    const char * inputFilename;  // index 0
    const char * outputFilename; // index 1
} clContext;

struct clImage;

#define clAllocate(BYTES) C->system.alloc(C, BYTES)
#define clAllocateStruct(T) (T *)C->system.alloc(C, sizeof(T))
#define clFree(P) C->system.free(C, P)
char * clContextStrdup(clContext * C, const char * str);

// Any/all of the clContextSystem struct can be NULL, including the struct itself. Any NULL values will use the default.
// No need to allocate the clContextSystem structure; just put it on the stack. Any values will be shallow copied.
clContext * clContextCreate(clContextSystem * system);
void clContextDestroy(clContext * C);

void clContextLog(clContext * C, const char * section, int indent, const char * format, ...);
void clContextLogError(clContext * C, const char * format, ...);

void clContextPrintSyntax(clContext * C);
void clContextPrintVersions(clContext * C);
void clContextPrintArgs(clContext * C);
clBool clContextParseArgs(clContext * C, int argc, char * argv[]);

struct clImage * clContextRead(clContext * C, const char * filename, const char * iccOverride, clFormat * outFormat);
clBool clContextWrite(clContext * C, struct clImage * image, const char * filename, clFormat format, int quality, int rate);

clBool clContextGetStockPrimaries(struct clContext * C, const char * name, struct clProfilePrimaries * outPrimaries);
clBool clContextGetRawStockPrimaries(struct clContext * C, const char * name, float outPrimaries[8]);

int clContextConvert(clContext * C);
int clContextGenerate(clContext * C);
int clContextIdentify(clContext * C);
int clContextModify(clContext * C);
int clContextReport(clContext * C);

#define TIMING_FORMAT "--> %g sec"
#define OVERALL_TIMING_FORMAT "==> %g sec"

// Enable on VS builds to dump memory leaks at the end of a debug session
// #define WIN32_MEMORY_LEAK_DETECTION

#endif // ifndef MAIN_H
