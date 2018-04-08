#ifndef COLORIST_PROFILE_H
#define COLORIST_PROFILE_H

#include "colorist/types.h"

typedef struct clProfilePrimaries
{
    float red[2];
    float green[2];
    float blue[2];
    float white[2];
} clProfilePrimaries;

typedef enum clProfileCurveType
{
    CL_PCT_NONE = 0,
    CL_PCT_GAMMA
} clProfileCurveType;

typedef struct clProfileCurve
{
    clProfileCurveType type;
    float gamma;
} clProfileCurve;

typedef struct clProfile
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;
    char * description;
    clRaw icc;
} clProfile;

typedef enum clProfileStock
{
    CL_PS_SRGB = 0,

    CL_PS_COUNT
} clProfileStock;

clProfile * clProfileCreateStock(clProfileStock stock);
clProfile * clProfileClone(clProfile * profile);
clProfile * clProfileCreate(clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance, const char * description);
clProfile * clProfileParse(const uint8_t * icc, int iccLen);
void clProfileDebugDump(clProfile * profile);
void clProfileDestroy(clProfile * profile);

#endif // ifndef COLORIST_PROFILE_H
