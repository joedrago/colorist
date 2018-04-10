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

clProfile * clProfileCreateStock(clProfileStock stock);
clProfile * clProfileClone(clProfile * profile);
clProfile * clProfileCreate(clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance, const char * description);
clProfile * clProfileCreateLinear(clProfile * origProfile);
clProfile * clProfileParse(const uint8_t * icc, int iccLen, const char * description);
clBool clProfileQuery(clProfile * profile, clProfilePrimaries * primaries, clProfileCurve * curve, int * maxLuminance);
clBool clProfilePack(clProfile * profile, clRaw * out);
void clProfileDebugDump(clProfile * profile);
void clProfileDestroy(clProfile * profile);

#endif // ifndef COLORIST_PROFILE_H
