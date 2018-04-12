// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef MAIN_H
#define MAIN_H

#include "colorist/colorist.h"

typedef enum Action
{
    ACTION_NONE = 0,
    ACTION_CONVERT,
    ACTION_GENERATE,
    ACTION_IDENTIFY,

    ACTION_ERROR
} Action;

typedef enum Format
{
    FORMAT_AUTO = 0,
    FORMAT_ICC,
    FORMAT_JP2,
    FORMAT_JPG,
    FORMAT_PNG,

    FORMAT_ERROR
} Format;

typedef enum Tonemap
{
    TONEMAP_AUTO = 0,
    TONEMAP_ON,
    TONEMAP_OFF
} Tonemap;

Format stringToFormat(const char * str);
const char * formatToString(Format format);
Format detectFormat(const char * filename);

typedef struct Args
{
    Action action;
    clBool autoGrade;            // -a
    int bpp;                     // -b
    const char * copyright;      // -c
    const char * description;    // -d
    Format format;               // -f
    float gamma;                 // -g
    clBool help;                 // -h
    int jobs;                    // -j
    int luminance;               // -l
    float primaries[8];          // -p
    int quality;                 // -q
    int rate;                    // -r
    Tonemap tonemap;             // -t
    clBool verbose;              // -v
    int rect[4];                 // -z
    const char * inputFilename;  // index 0
    const char * outputFilename; // index 1
} Args;

clImage * readImage(const char * filename, Format * outFormat);
clBool writeImage(clImage * image, const char * filename, Format format, int quality, int rate);

char * generateDescription(clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance);

int actionConvert(Args * args);
int actionGenerate(Args * args);
int actionIdentify(Args * args);

#endif // ifndef MAIN_H
