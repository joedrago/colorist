// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/profile.h"

#include <string.h>

int clContextModify(clContext * C)
{
    clProfile * profile = NULL;
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int luminance = 0;
    clBool checkColorantsAndGamma = clFalse;

    clContextLog(C, "action", 0, "Modify: %s -> %s", C->inputFilename, C->outputFilename);

    profile = clProfileRead(C, C->inputFilename);
    if (!profile) {
        clContextLogError(C, "Cannot parse ICC profile: %s", C->inputFilename);
        goto cleanup;
    }
    clContextLog(C, "modify", 0, "Loaded profile: %s", C->inputFilename);
    if (!clProfileQuery(C, profile, &primaries, &curve, &luminance)) {
        clContextLogError(C, "Cannot query ICC profile: %s", C->outputFilename);
        goto cleanup;
    }
    clProfileDebugDump(C, profile, clTrue, 0);

    if (C->params.copyright) {
        clContextLog(C, "modify", 0, "Setting copyright: \"%s\"", C->params.copyright);
        clProfileSetMLU(C, profile, "cprt", "en", "US", C->params.copyright);
    }
    if (C->params.description) {
        clContextLog(C, "modify", 0, "Setting description: \"%s\"", C->params.description);
        clProfileSetMLU(C, profile, "desc", "en", "US", C->params.description);
    }
    if (C->params.primaries[0] > 0.0f) {
        clContextLogError(C, "Altering primaries (-p) is currenly unsupported, use generate to make a fresh ICC profile instead");
        goto cleanup;
    }
    if (C->params.gamma > 0.0f) {
        checkColorantsAndGamma = clTrue;
        clProfileRemoveTag(C, profile, "A2B0", "changing gamma");
        clProfileRemoveTag(C, profile, "A2B1", "changing gamma");
        clProfileRemoveTag(C, profile, "B2A0", "changing gamma");
        clProfileRemoveTag(C, profile, "B2A1", "changing gamma");
        clProfileReload(C, profile);
        clContextLog(C, "modify", 0, "Setting gamma: %g", C->params.gamma);
        if (!clProfileSetGamma(C, profile, C->params.gamma)) {
            clContextLogError(C, "Cannot set gamma: %g", C->params.gamma);
            goto cleanup;
        }
    }
    if (C->params.luminance > 0) {
        clContextLog(C, "modify", 0, "Setting luminance: %d", C->params.luminance);
        if (!clProfileSetLuminance(C, profile, C->params.luminance)) {
            clContextLogError(C, "Cannot set luminance: %d", C->params.luminance);
            goto cleanup;
        }
    }

    if (C->params.stripTags) {
        char * tagsBuffer = clContextStrdup(C, C->params.stripTags);
        char * tagName;
        for (tagName = strtok(tagsBuffer, ","); tagName != NULL; tagName = strtok(NULL, ",")) {
            if (clProfileRemoveTag(C, profile, tagName, NULL)) {
                clContextLog(C, "modify", 0, "Stripping tag: '%s'", tagName);
            } else {
                clContextLog(C, "modify", 0, "Tag '%s' already absent, skipping strip", tagName);
            }
        }
        clProfileReload(C, profile);
        clFree(tagsBuffer);
    }

    clContextLog(C, "modify", 0, "Writing profile: %s", C->outputFilename);
    clProfileDebugDump(C, profile, clTrue, 0);

    if (!clProfileWrite(C, profile, C->outputFilename)) {
        clContextLogError(C, "Cannot write ICC profile: %s", C->outputFilename);
        goto cleanup;
    }

cleanup:
    if (profile)
        clProfileDestroy(C, profile);
    return 0;
}
