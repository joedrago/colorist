// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_PROFILE_H
#define COLORIST_PROFILE_H

#include "colorist/raw.h"
#include "colorist/types.h"

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
    CL_PCT_PQ,
    CL_PCT_COMPLEX
} clProfileCurveType;
const char * clProfileCurveTypeToString(struct clContext * C, clProfileCurveType curveType);

typedef struct clProfileCurve
{
    clProfileCurveType type;
    float implicitScale;
    float gamma;
} clProfileCurve;

typedef struct clProfile
{
    char * description;
    cmsHPROFILE handle;
    clRaw raw;             // Populated during clProfileParse(), preferred during clProfilePack(), cleared on any clProfileSet*() call
    uint8_t signature[16]; // Populated during clProfileParse()
    clBool ccmm;           // Can this profile be used by colorist's built-in CMM? (if false for either src or dst, LittleCMS is used)
} clProfile;

typedef enum clProfileStock
{
    CL_PS_SRGB = 0
} clProfileStock;

clProfile * clProfileCreateStock(struct clContext * C, clProfileStock stock);
clProfile * clProfileClone(struct clContext * C, clProfile * profile);
clProfile * clProfileCreate(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance, const char * description);
clProfile * clProfileParse(struct clContext * C, const uint8_t * icc, size_t iccLen, const char * description);
clProfile * clProfileRead(struct clContext * C, const char * filename);
clBool clProfileReload(struct clContext * C, clProfile * profile);
clBool clProfileWrite(struct clContext * C, clProfile * profile, const char * filename);
clBool clProfileQuery(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries, clProfileCurve * curve, int * luminance);
clBool clProfileHasPQSignature(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries);
clBool clProfileCurveHasPQSignature(struct clContext * C, clProfile * profile);
char * clProfileGetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3]);
clBool clProfileSetMLU(struct clContext * C, clProfile * profile, const char tag[5], const char languageCode[3], const char countryCode[3], const char * ascii);
clBool clProfilePack(struct clContext * C, clProfile * profile, struct clRaw * out);
clBool clProfileSetGamma(struct clContext * C, clProfile * profile, float gamma);
clBool clProfileSetLuminance(struct clContext * C, clProfile * profile, int luminance);
clBool clProfileRemoveTag(struct clContext * C, clProfile * profile, char * tag, const char * reason);
clBool clProfileMatches(struct clContext * C, clProfile * profile1, clProfile * profile2);
clBool clProfileUsesCCMM(struct clContext * C, clProfile * profile);
const char * clProfileCMMName(struct clContext * C, clProfile * profile); // Convenience function
size_t clProfileSize(struct clContext * C, clProfile * profile);
void clProfileDebugDump(struct clContext * C, clProfile * profile, clBool dumpTags, int extraIndent);
void clProfileDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clProfile * profile, clBool dumpTags);
void clProfileDestroy(struct clContext * C, clProfile * profile);

clBool clProfilePrimariesMatch(struct clContext * C, clProfilePrimaries * p1, clProfilePrimaries * p2);

// TODO: this needs a better name
char * clGenerateDescription(struct clContext * C, clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance);

#endif // ifndef COLORIST_PROFILE_H
