// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_PROFILE_H
#define COLORIST_PROFILE_H

#include "colorist/types.h"

#include "lcms2.h"

struct clContext;

typedef struct clProfilePrimaries
{
    float red[2];
    float green[2];
    float blue[2];
    float white[2];
} clProfilePrimaries;

typedef enum clProfileCurveType
{
    CL_PCT_UNKNOWN = 0,
    CL_PCT_GAMMA,
    CL_PCT_COMPLEX
} clProfileCurveType;

typedef struct clProfileCurve
{
    clProfileCurveType type;
    float gamma;
} clProfileCurve;

typedef struct clProfile
{
    char * description;
    cmsHPROFILE handle;
} clProfile;

typedef enum clProfileStock
{
    CL_PS_SRGB = 0,

    CL_PS_COUNT
} clProfileStock;

clProfile * clProfileCreateStock(struct clContext * C, clProfileStock stock);
clProfile * clProfileClone(struct clContext * C, clProfile * profile);
clProfile * clProfileCreate(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance, const char * description);
clProfile * clProfileParse(struct clContext * C, const uint8_t * icc, int iccLen, const char * description);
clProfile * clProfileRead(struct clContext * C, const char * filename);
clBool clProfileWrite(struct clContext * C, clProfile * profile, const char * filename);
clBool clProfileQuery(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries, clProfileCurve * curve, int * maxLuminance);
char * clProfileGetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3]);
clBool clProfileSetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3], const char * ascii);
clBool clProfilePack(struct clContext * C, clProfile * profile, clRaw * out);
void clProfileDebugDump(struct clContext * C, clProfile * profile, int extraIndent);
void clProfileDestroy(struct clContext * C, clProfile * profile);

// TODO: this needs a better name
char * clGenerateDescription(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance);

#endif // ifndef COLORIST_PROFILE_H
