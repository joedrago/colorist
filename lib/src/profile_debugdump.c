// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/profile.h"

#include "colorist/context.h"

#include <string.h>

static const char * curveTypeToString(clProfileCurveType curveType)
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

void clProfileDebugDump(struct clContext * C, clProfile * profile, clBool dumpTags, int extraIndent)
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance;
    char * tempStr;

    if (clProfileQuery(C, profile, &primaries, &curve, &luminance)) {
        clContextLog(C, "profile", 0 + extraIndent, "Profile \"%s\"", profile->description);
        clContextLog(C, "profile", 1 + extraIndent, "Size: %d bytes", clProfileSize(C, profile));

        if (tempStr = clProfileGetMLU(C, profile, "cprt", "en", "US")) {
            clContextLog(C, "profile", 1 + extraIndent, "Copyright: \"%s\"", tempStr);
            clFree(tempStr);
        }

        clContextLog(C, "profile", 1 + extraIndent, "Primaries: (r:%.4g,%.4g g:%.4g,%.4g b:%.4g,%.4g w:%.4g,%.4g)",
            primaries.red[0], primaries.red[1],
            primaries.green[0], primaries.green[1],
            primaries.blue[0], primaries.blue[1],
            primaries.white[0], primaries.white[1]);
        clContextLog(C, "profile", 1 + extraIndent, "Max Luminance: %d", luminance);
        clContextLog(C, "profile", 1 + extraIndent, "Curve: %s(%.3g)", curveTypeToString(curve.type), curve.gamma);
        if (curve.matrixCurveScale > 0.0f) {
            clContextLog(C, "profile", 1 + extraIndent, "Implicit matrix curve scale: %g", curve.matrixCurveScale);
            clContextLog(C, "profile", 1 + extraIndent, "Actual max luminance: %g", luminance * curve.matrixCurveScale);
        }

        if (dumpTags) {
            // Dump tags
            cmsInt32Number i, tagSize, tagCount = cmsGetTagCount(profile->handle);
            if (tagCount > 0) {
                clContextLog(C, "profile", 1 + extraIndent, "Tags [%d]:", tagCount);
            }
            for (i = 0; i < tagCount; ++i) {
                char tagName[5];
                cmsTagSignature invSig, sig = cmsGetTagSignature(profile->handle, i);
                invSig = clNTOHL(sig);
                tagSize = cmsReadRawTag(profile->handle, sig, NULL, 0);
                memcpy(tagName, &invSig, 4);
                tagName[4] = 0;
                clContextLog(C, "profile", 2 + extraIndent, "Tag %2d [%5d bytes]: %s", i, tagSize, tagName);
            }
        }
    }
}
