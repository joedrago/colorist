// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/profile.h"

#include "colorist/context.h"

#include <string.h>

struct PQProfile
{
    uint8_t signature[16];
    const char * stockPrimariesName;
};

static struct PQProfile pqProfiles_[] = {
    { { 0x59, 0x53, 0xac, 0x21, 0x04, 0x41, 0x70, 0xc4, 0x7c, 0x98, 0x9e, 0xa6, 0x27, 0x11, 0x42, 0xd9 }, "bt709" }, // HDR_HD_ST2084.icc
    { { 0x57, 0x15, 0xa6, 0x9d, 0xc0, 0xc9, 0x89, 0x16, 0x1e, 0x3f, 0x71, 0x6a, 0xe3, 0x72, 0xa0, 0x1d }, "p3" },    // HDR_P3_D65_ST2084.icc
    { { 0xbf, 0x0c, 0x50, 0x8c, 0x59, 0xaa, 0xfc, 0xa1, 0x17, 0xc3, 0xcf, 0xce, 0xd6, 0xf3, 0xe3, 0x07 }, "bt2020" } // HDR_UHD_ST2084.icc
};
static const int pqProfileCount = sizeof(pqProfiles_) / sizeof(pqProfiles_[0]);

clBool clProfileHasPQSignature(struct clContext * C, clProfile * profile, clProfilePrimaries * primaries)
{
    int i;
    for (i = 0; i < pqProfileCount; ++i) {
        struct PQProfile * pqProfile = &pqProfiles_[i];
        if (!memcmp(pqProfile->signature, profile->signature, 16)) {
            clContextGetStockPrimaries(C, pqProfile->stockPrimariesName, primaries);
            return clTrue;
        }
    }
    return clFalse;
}
