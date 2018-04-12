// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/profile.h"

#include <stdio.h>

const char * curveTypeToString(clProfileCurveType curveType)
{
    switch (curveType) {
        case CL_PCT_GAMMA:   return "Gamma";
        case CL_PCT_COMPLEX: return "Complex";
        case CL_PCT_UNKNOWN:
        default:
            break;
    }
    return "Unknown";
}

void clProfileDebugDump(clProfile * profile)
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance;

    if (clProfileQuery(profile, &primaries, &curve, &luminance)) {
        printf("  Profile \"%s\" pri:(r:%.4g,%.4g g:%.4g,%.4g b:%.4g,%.4g w:%.4g,%.4g) maxLum:%d curve:%s(%.2g)\n",
            profile->description,
            primaries.red[0], primaries.red[1],
            primaries.green[0], primaries.green[1],
            primaries.blue[0], primaries.blue[1],
            primaries.white[0], primaries.white[1],
            luminance,
            curveTypeToString(curve.type),
            curve.gamma
            );
    }
}
