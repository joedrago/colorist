// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/profile.h"

#include "colorist/context.h"

#include <string.h>

#include "cJSON.h"

static const char * curveTypeToString(clProfileCurveType curveType)
{
    switch (curveType) {
        case CL_PCT_GAMMA:   return "gamma";
        case CL_PCT_COMPLEX: return "complex";
        case CL_PCT_UNKNOWN:
        default:
            break;
    }
    return "unknown";
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

        tempStr = clProfileGetMLU(C, profile, "cprt", "en", "US");
        if (tempStr) {
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

void clProfileDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clProfile * profile, clBool dumpTags)
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance;
    char * tempStr;

    if (clProfileQuery(C, profile, &primaries, &curve, &luminance)) {
        cJSON * jsonPrimaries;
        cJSON * jsonPrimary;
        cJSON * jsonCurve;

        cJSON_AddStringToObject(jsonOutput, "description", profile->description);
        cJSON_AddNumberToObject(jsonOutput, "size", clProfileSize(C, profile));

        tempStr = clProfileGetMLU(C, profile, "cprt", "en", "US");
        if (tempStr) {
            cJSON_AddStringToObject(jsonOutput, "copyright", tempStr);
            clFree(tempStr);
        }

        jsonPrimaries = cJSON_AddObjectToObject(jsonOutput, "primaries");
        jsonPrimary = cJSON_AddObjectToObject(jsonPrimaries, "red");
        cJSON_AddNumberToObject(jsonPrimary, "x", primaries.red[0]);
        cJSON_AddNumberToObject(jsonPrimary, "y", primaries.red[1]);
        jsonPrimary = cJSON_AddObjectToObject(jsonPrimaries, "green");
        cJSON_AddNumberToObject(jsonPrimary, "x", primaries.green[0]);
        cJSON_AddNumberToObject(jsonPrimary, "y", primaries.green[1]);
        jsonPrimary = cJSON_AddObjectToObject(jsonPrimaries, "blue");
        cJSON_AddNumberToObject(jsonPrimary, "x", primaries.blue[0]);
        cJSON_AddNumberToObject(jsonPrimary, "y", primaries.blue[1]);
        jsonPrimary = cJSON_AddObjectToObject(jsonPrimaries, "white");
        cJSON_AddNumberToObject(jsonPrimary, "x", primaries.white[0]);
        cJSON_AddNumberToObject(jsonPrimary, "y", primaries.white[1]);

        cJSON_AddNumberToObject(jsonOutput, "luminance", luminance);

        jsonCurve = cJSON_AddObjectToObject(jsonOutput, "curve");
        cJSON_AddStringToObject(jsonCurve, "type", curveTypeToString(curve.type));
        cJSON_AddNumberToObject(jsonCurve, "gamma", curve.gamma);
        cJSON_AddNumberToObject(jsonCurve, "matrixCurveScale", curve.matrixCurveScale);
        if (curve.matrixCurveScale > 0.0f) {
            cJSON_AddNumberToObject(jsonOutput, "actualLuminance", luminance * curve.matrixCurveScale);
        } else {
            cJSON_AddNumberToObject(jsonOutput, "actualLuminance", luminance);
        }

        if (dumpTags) {
            cJSON * jsonTags = cJSON_AddArrayToObject(jsonOutput, "tags");
            cmsInt32Number i, tagSize, tagCount = cmsGetTagCount(profile->handle);
            for (i = 0; i < tagCount; ++i) {
                cJSON * jsonTag;
                char tagName[5];
                cmsTagSignature invSig, sig = cmsGetTagSignature(profile->handle, i);
                invSig = clNTOHL(sig);
                tagSize = cmsReadRawTag(profile->handle, sig, NULL, 0);
                memcpy(tagName, &invSig, 4);
                tagName[4] = 0;

                jsonTag = cJSON_CreateObject();
                cJSON_AddStringToObject(jsonTag, "name", tagName);
                cJSON_AddNumberToObject(jsonTag, "size", tagSize);
                cJSON_AddItemToArray(jsonTags, jsonTag);
            }
        }
    }
}
