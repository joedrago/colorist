// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_PROFILE_H
#define COLORIST_PROFILE_H

#include "colorist/types.h"
#include "colorist/raw.h"

#include "lcms2.h"

struct clContext;
struct cJSON;

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
    float matrixCurveScale; // currently only used for reporting
    float gamma;
} clProfileCurve;

typedef struct clProfile
{
    char * description;
    cmsHPROFILE handle;
    uint8_t signature[16]; // Populated during clProfileParse()
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
clBool clProfileReload(struct clContext * C, clProfile * profile);
clBool clProfileWrite(struct clContext * C, clProfile * profile, const char * filename);
clBool clProfileQuery(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries, clProfileCurve * curve, int * maxLuminance);
clBool clProfileHasPQSignature(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries);
char * clProfileGetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3]);
clBool clProfileSetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3], const char * ascii);
clBool clProfilePack(struct clContext * C, clProfile * profile, struct clRaw * out);
clBool clProfileSetGamma(struct clContext * C, clProfile * profile, float gamma);
clBool clProfileSetLuminance(struct clContext * C, clProfile * profile, int luminance);
int clProfileSize(struct clContext * C, clProfile * profile);
void clProfileDebugDump(struct clContext * C, clProfile * profile, clBool dumpTags, int extraIndent);
void clProfileDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clProfile * profile, clBool dumpTags);
void clProfileDestroy(struct clContext * C, clProfile * profile);

// TODO: this needs a better name
char * clGenerateDescription(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance);

#endif // ifndef COLORIST_PROFILE_H
