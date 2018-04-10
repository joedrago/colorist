// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

#include <string.h>

int actionConvert(Args * args)
{
    Timer overall, t;
    clImage * srcImage = NULL;
    clProfile * dstProfile = NULL;
    clImage * dstImage = NULL;
    Format outputFileFormat;
    clBool writeSuccess;

    outputFileFormat = args->format;
    if (outputFileFormat == FORMAT_AUTO)
        outputFileFormat = detectFormat(args->outputFilename);
    if (outputFileFormat == FORMAT_ERROR) {
        fprintf(stderr, "ERROR: Unknown output file format: %s\n", args->outputFilename);
        return 1;
    }

    printf("Colorist [convert]: %s -> %s\n", args->inputFilename, args->outputFilename);
    timerStart(&overall);

    printf("Reading: %s\n", args->inputFilename);
    timerStart(&t);
    srcImage = readImage(args->inputFilename, NULL);
    if (srcImage == NULL) {
        return 1;
    }
    printf("    done (%g sec).\n\n", timerElapsedSeconds(&t));

    // Create the destination profile, or clone the source one
    {
        if (
            (args->primaries[0] > 0.0f) ||
            (args->gamma > 0.0f) ||
            (args->luminance != 0) ||
            (args->description) ||
            (args->copyright)
            )
        {
            clProfilePrimaries primaries;
            clProfileCurve curve;
            int luminance;
            char * description = NULL;
            if (!clProfileQuery(srcImage->profile, &primaries, &curve, &luminance)) {
                fprintf(stderr, "ERROR: Can't parse source image's ICC profile\n");
                return 1;
            }
            if (args->primaries[0] > 0.0f) {
                primaries.red[0] = args->primaries[0];
                primaries.red[1] = args->primaries[1];
                primaries.green[0] = args->primaries[2];
                primaries.green[1] = args->primaries[3];
                primaries.blue[0] = args->primaries[4];
                primaries.blue[1] = args->primaries[5];
                primaries.white[0] = args->primaries[6];
                primaries.white[1] = args->primaries[7];
            }
            if (args->gamma > 0.0f) {
                curve.type = CL_PCT_GAMMA;
                curve.gamma = args->gamma;
            } else {
                if (curve.type != CL_PCT_GAMMA) {
                    // TODO: Support/pass-through any source curve
                    fprintf(stderr, "ERROR: Can't create destination profile, source profile's curve cannot be re-created as it isn't just a simple gamma curve\n");
                    return 1;
                }
            }
            if (args->luminance != 0) {
                luminance = args->luminance;
            }
            if (args->description) {
                description = strdup(args->description);
            } else {
                // TODO: Come up with a good description
                description = strdup(srcImage->profile->description);
            }

            printf("Creating new destination ICC profile: \"%s\"\n", description);
            dstProfile = clProfileCreate(&primaries, &curve, luminance, description);
            free(description);

            if (args->copyright) {
                printf("Setting copyright: \"%s\"\n", args->copyright);
                clProfileSetMLU(dstProfile, "cprt", "en", "US", args->copyright);
            }
        } else {
            // just clone the source one

            printf("Using source ICC profile: \"%s\"\n", srcImage->profile->description);
            dstProfile = clProfileClone(srcImage->profile);
        }
    }

    if (outputFileFormat == FORMAT_ICC) {
        // Just dump out the profile to disk and bail out

        printf("Writing ICC: %s\n", args->outputFilename);
        clProfileDebugDump(dstProfile);

        if (!clProfileWrite(dstProfile, args->outputFilename)) {
            return 1;
        }
        clImageDestroy(srcImage);
        clProfileDestroy(dstProfile);
        printf("\nConversion complete (%g sec).\n", timerElapsedSeconds(&overall));
        return 0;
    }

    // Create dstImage
    {
        int depth = args->bpp;
        if (depth == 0) {
            depth = srcImage->depth;
        }
        dstImage = clImageCreate(srcImage->width, srcImage->height, depth, dstProfile);
    }

    // Show image details
    {
        printf("\n");
        printf("Src Image:\n");
        clImageDebugDump(srcImage, 0, 0, 0, 0);
        printf("\n");
        printf("Dst Image:\n");
        clImageDebugDump(dstImage, 0, 0, 0, 0);
        printf("\n");
    }

    // Convert srcImage -> dstImage
    {
        cmsUInt32Number srcFormat = (srcImage->depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;
        cmsUInt32Number dstFormat = (dstImage->depth == 16) ? TYPE_RGBA_16 : TYPE_RGBA_8;

        int srcLuminance = 0;
        int dstLuminance = 0;
        clProfileQuery(srcImage->profile, NULL, NULL, &srcLuminance);
        clProfileQuery(dstImage->profile, NULL, NULL, &dstLuminance);
        srcLuminance = (srcLuminance != 0) ? srcLuminance : COLORIST_DEFAULT_LUMINANCE;
        dstLuminance = (dstLuminance != 0) ? dstLuminance : COLORIST_DEFAULT_LUMINANCE;
        if (srcLuminance == dstLuminance) {
            cmsHTRANSFORM directTransform;

            printf("Converting directly (luminances match)...\n");
            timerStart(&t);
            directTransform = cmsCreateTransform(srcImage->profile->handle, srcFormat, dstImage->profile->handle, dstFormat, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);
            cmsDoTransform(directTransform, srcImage->pixels, dstImage->pixels, dstImage->width * dstImage->height);
            printf("    done (%g sec).\n\n", timerElapsedSeconds(&t));
        } else {
            clProfile * dstLinear;
            cmsHTRANSFORM toLinear, fromLinear;
            int pixelCount;
            float * floatPixels = NULL;

            dstLinear = clProfileCreateLinear(dstImage->profile);
            if (!dstLinear) {
                fprintf(stderr, "ERROR: Can't create destination linear profile\n");
                return 1;
            }

            pixelCount = dstImage->width * dstImage->height;
            floatPixels = malloc(4 * sizeof(float) * pixelCount);
            toLinear = cmsCreateTransform(srcImage->profile->handle, srcFormat, dstLinear->handle, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);
            fromLinear = cmsCreateTransform(dstLinear->handle, TYPE_RGBA_FLT, dstImage->profile->handle, dstFormat, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);

            printf("Converting to floating point...\n");
            timerStart(&t);
            cmsDoTransform(toLinear, srcImage->pixels, floatPixels, pixelCount);
            printf("    done (%g sec).\n\n", timerElapsedSeconds(&t));

            // Luminance scaling / tonemapping
            {
                float luminanceScale = (float)srcLuminance / (float)dstLuminance;
                float * pixel;
                int i;

                timerStart(&t);
                if (luminanceScale <= 1.0f) {
                    printf("Scaling luminance (basic)...\n");
                    pixel = floatPixels;
                    for (i = 0; i < pixelCount; ++i) {
                        pixel[0] *= luminanceScale;
                        pixel[1] *= luminanceScale;
                        pixel[2] *= luminanceScale;
                        pixel += 4;
                    }
                } else {
                    printf("Scaling luminance (tonemapping)...\n");
                    pixel = floatPixels;
                    for (i = 0; i < pixelCount; ++i) {
                        pixel[0] *= luminanceScale;                     // scale
                        pixel[0] = (pixel[0] < 0.0f) ? 0.0f : pixel[0]; // max(0, v)
                        pixel[0] = pixel[0] / (1.0f + pixel[0]);        // reinhard tonemap
                        pixel[1] *= luminanceScale;                     // scale
                        pixel[1] = (pixel[1] < 0.0f) ? 0.0f : pixel[1]; // max(0, v)
                        pixel[1] = pixel[1] / (1.0f + pixel[1]);        // reinhard tonemap
                        pixel[2] *= luminanceScale;                     // scale
                        pixel[2] = (pixel[2] < 0.0f) ? 0.0f : pixel[2]; // max(0, v)
                        pixel[2] = pixel[2] / (1.0f + pixel[2]);        // reinhard tonemap
                        pixel += 4;
                    }
                }
                printf("    done (%g sec).\n\n", timerElapsedSeconds(&t));
            }

            printf("Converting from floating point...\n");
            timerStart(&t);
            cmsDoTransform(fromLinear, floatPixels, dstImage->pixels, pixelCount);
            printf("    done (%g sec).\n\n", timerElapsedSeconds(&t));

            free(floatPixels);
            clProfileDestroy(dstLinear);
        }
    }

    switch (outputFileFormat) {
        case FORMAT_JP2:
            printf("Writing JP2 [q:%d, rate:%dx]: %s\n", args->quality, args->rate, args->outputFilename);
            break;
        case FORMAT_JPG:
            printf("Writing JPG [q:%d]: %s\n", args->quality, args->outputFilename);
            break;
        default:
            printf("Writing: %s\n", args->outputFilename);
            break;
    }
    timerStart(&t);
    writeSuccess = writeImage(dstImage, args->outputFilename, outputFileFormat, args->quality, args->rate);
    printf("    done (%g sec).\n\n", timerElapsedSeconds(&t));
    clImageDestroy(srcImage);
    clImageDestroy(dstImage);
    if (!writeSuccess)
        return 1;

    printf("\nConversion complete (%g sec).\n", timerElapsedSeconds(&overall));
    return 0;
}
