// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/colorist.h"

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>

static void clContextSilentLog(clContext * C, const char * section, int indent, const char * format, va_list args)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(section);
    COLORIST_UNUSED(indent);
    COLORIST_UNUSED(format);
    COLORIST_UNUSED(args);
}
static void clContextSilentLogError(clContext * C, const char * format, va_list args)
{
    COLORIST_UNUSED(C);
    COLORIST_UNUSED(format);
    COLORIST_UNUSED(args);
}

int main(int argc, char * argv[])
{
    if (argc < 2) {
        printf("colorist-benchmark [input image filename] [optional attempts]\n");
        return 1;
    }

    int attempts = 1;
    if (argc > 2) {
        attempts = atoi(argv[2]);
        if (attempts < 1) {
            attempts = 1;
        }
    }

    clContextSystem silentSystem;
    silentSystem.alloc = clContextDefaultAlloc;
    silentSystem.free = clContextDefaultFree;
    silentSystem.log = clContextSilentLog;
    silentSystem.error = clContextSilentLogError;

    const char * inputFilename = argv[1];
    clContext * C = clContextCreate(&silentSystem);
    struct clImage * image = NULL;

    int width = 0;
    int height = 0;
    int depth = 0;
    const char * error = "true";
    int size = (int)clFileSize(inputFilename);

    double elapsedTotal = 0.0;
    double elapsedCodec = 0.0;
    double elapsedYUV = 0.0;
    double elapsedFill = 0.0;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        Timer t;
        timerStart(&t);
        image = clContextRead(C, inputFilename, NULL, NULL);
        elapsedTotal += timerElapsedSeconds(&t);
        elapsedCodec += C->readExtraInfo.decodeCodecSeconds;
        elapsedYUV += C->readExtraInfo.decodeYUVtoRGBSeconds;
        elapsedFill += C->readExtraInfo.decodeFillSeconds;
        if (image) {
            width = image->width;
            height = image->height;
            depth = image->depth;
            error = "false";
        } else {
            break;
        }
    }
    if (attempts > 1) {
        elapsedTotal /= (double)attempts;
        elapsedCodec /= (double)attempts;
        elapsedYUV /= (double)attempts;
        elapsedFill /= (double)attempts;
    }

    printf("{ \"elapsedTotal\": %f, \"elapsedCodec\": %f, \"elapsedYUV\": %f, \"elapsedFill\": %f, \"size\": %d, \"width\": %d, \"height\": %d, \"depth\": %d, \"attempts\": %d, \"error\": %s }\n",
           elapsedTotal,
           elapsedCodec,
           elapsedYUV,
           elapsedFill,
           size,
           width,
           height,
           depth,
           attempts,
           error);

    if (image) {
        clImageDestroy(C, image);
    }
    clContextDestroy(C);
    return 0;
}
