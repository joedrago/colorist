#include "colorist/profile.h"

#include <string.h>

clProfile * clProfileParse(const uint8_t * icc, int iccLen)
{
    clProfile * profile;
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;
    const char * description;

    // TODO: actually parse ICC
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
    description = "Parsed";

    // Create the profile
    profile = clProfileCreate(&primaries, &curve, maxLuminance, description);

    // Stash off original ICC block for perfect pass-through
    clRawSet(&profile->icc, icc, iccLen);

    return profile;
}
