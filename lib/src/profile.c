#include "colorist/profile.h"

#include <math.h>
#include <string.h>

static void normalizeProfile(clProfile * profile);

clProfile * clProfileCreateStock(clProfileStock stock)
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;
    const char * description;
    COLORIST_ASSERT((stock >= 0) && (stock < CL_PS_COUNT));
    switch (stock) {
        case CL_PS_SRGB:
        default:
        {
            primaries.red[0] = 0.64f;
            primaries.red[1] = 0.33f;
            primaries.green[0] = 0.30f;
            primaries.green[1] = 0.60f;
            primaries.blue[0] = 0.15f;
            primaries.blue[1] = 0.06f;
            primaries.white[0] = 0.3127f;
            primaries.white[1] = 0.3290f;
            curve.type = CL_PCT_GAMMA;
            curve.gamma = 2.4f;
            maxLuminance = 300;
            description = "SRGB";
            break;
        }
    }
    return clProfileCreate(&primaries, &curve, maxLuminance, description);
}

clProfile * clProfileClone(clProfile * profile)
{
    clProfile * clone = clProfileCreate(&profile->primaries, &profile->curve, profile->maxLuminance, profile->description);
    clRawClone(&clone->icc, &profile->icc);
    return clone;
}

clProfile * clProfileCreate(clProfilePrimaries * primaries, clProfileCurve * curve, int maxLuminance, const char * description)
{
    clProfile * profile = clAllocate(clProfile);
    memcpy(&profile->primaries, primaries, sizeof(clProfilePrimaries));
    memcpy(&profile->curve, curve, sizeof(clProfileCurve));
    profile->maxLuminance = maxLuminance;
    profile->description = strdup(description);
    normalizeProfile(profile);
    return profile;
}

void clProfileDestroy(clProfile * profile)
{
    free(profile->description);
    clRawFree(&profile->icc);
    free(profile);
}

static float roundTo4(float f)
{
    return roundf(f * 10000.0f) / 10000.0f;
}

static float roundTo2(float f)
{
    return roundf(f * 100.0f) / 100.0f;
}

static void normalizeProfile(clProfile * profile)
{
    profile->primaries.red[0] = roundTo4(profile->primaries.red[0]);
    profile->primaries.red[1] = roundTo4(profile->primaries.red[1]);
    profile->primaries.green[0] = roundTo4(profile->primaries.green[0]);
    profile->primaries.green[1] = roundTo4(profile->primaries.green[1]);
    profile->primaries.blue[0] = roundTo4(profile->primaries.blue[0]);
    profile->primaries.blue[1] = roundTo4(profile->primaries.blue[1]);
    profile->primaries.white[0] = roundTo4(profile->primaries.white[0]);
    profile->primaries.white[1] = roundTo4(profile->primaries.white[1]);
    if (profile->curve.type == CL_PCT_GAMMA) {
        profile->curve.gamma = roundTo2(profile->curve.gamma);
    } else {
        profile->curve.gamma = 0.0f;
    }
}
