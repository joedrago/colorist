// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/profile.h"

#include <stdio.h>

void clProfileDebugDump(clProfile * profile)
{
    printf("  Profile \"%s\" pri:(r:%4.4g,%4.4g g:%4.4g,%4.4g b:%4.4g,%4.4g w:%4.4g,%4.4g) maxLum:%d curve:%s(%2.2g)\n",
        profile->description,
        profile->primaries.red[0], profile->primaries.red[1],
        profile->primaries.green[0], profile->primaries.green[1],
        profile->primaries.blue[0], profile->primaries.blue[1],
        profile->primaries.white[0], profile->primaries.white[1],
        profile->maxLuminance,
        (profile->curve.type == CL_PCT_GAMMA) ? "Gamma" : "None",
        profile->curve.gamma
        );
}
