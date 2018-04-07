#include "colorist/colorist.h"

#include "png.h"
#include "zlib.h"
#include "openjpeg.h"

const char * clVersion()
{
    // return zlibVersion();
    // return png_get_header_version(NULL);
    return opj_version();
}
