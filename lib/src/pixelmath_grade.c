#include "colorist/pixelmath.h"

#include "colorist/task.h"

#include <math.h>

// NOTE: This is a work in progress. There are probably lots of problems with this.

// roundf() doesn't exist until C99
float clPixelMathRoundf(float val)
{
    return floorf(val + 0.5f);
}

static float gammaErrorTerm(float gamma, float * pixels, int pixelCount, float maxChannel, float luminanceScale)
{
    float invGamma = 1.0f / gamma;
    float errorTerm = 0.0f;
    float scaledChannel;
    float * pixel = pixels;
    int i;

    for (i = 0; i < pixelCount; ++i) {
        scaledChannel = pixel[0] * luminanceScale;
        scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
        errorTerm += fabsf(scaledChannel - powf(clPixelMathRoundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gamma));

        scaledChannel = pixel[1] * luminanceScale;
        scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
        errorTerm += fabsf(scaledChannel - powf(clPixelMathRoundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gamma));

        scaledChannel = pixel[2] * luminanceScale;
        scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
        errorTerm += fabsf(scaledChannel - powf(clPixelMathRoundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gamma));

        pixel += 4;
    }
    return errorTerm;
}

typedef struct clGammaErrorTermTask
{
    int gammaInt;
    float gamma;
    float * pixels;
    int pixelCount;
    float maxChannel;
    float luminanceScale;
    float outErrorTerm;
} clGammaErrorTermTask;

static void gammaErrorTermTaskFunc(clGammaErrorTermTask * info)
{
    info->outErrorTerm = gammaErrorTerm(info->gamma, info->pixels, info->pixelCount, info->maxChannel, info->luminanceScale);
}

void clPixelMathColorGrade(int taskCount, float * pixels, int pixelCount, int srcLuminance, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose)
{
    int maxLuminance = 0;
    float bestGamma = 0.0f;
    float * pixel;
    int i;

    // Find max luminance
    if (*outLuminance == 0) {
        // TODO: This should probably be some kind of histogram which spends most of the codepoints where most of the pixel values are.

        float maxChannel = 0.0f;

        pixel = pixels;
        for (i = 0; i < pixelCount; ++i) {
            maxChannel = (maxChannel > pixel[0]) ? maxChannel : pixel[0];
            maxChannel = (maxChannel > pixel[1]) ? maxChannel : pixel[1];
            maxChannel = (maxChannel > pixel[2]) ? maxChannel : pixel[2];
            pixel += 4;
        }

        maxLuminance = (int)(maxChannel * srcLuminance);
        maxLuminance = CL_CLAMP(maxLuminance, 0, srcLuminance);
        printf(" * [grading] Found max luminance: %d nits\n", maxLuminance);
    } else {
        maxLuminance = *outLuminance;
        printf(" * [grading] Using requested max luminance: %d nits\n", maxLuminance);
    }

    // Find best gamma
    if (*outGamma == 0.0f) {
        float luminanceScale = (float)srcLuminance / maxLuminance;
        int gammaInt;
        int minGammaInt;
        float minErrorTerm = -1.0f;
        float maxChannel = (dstColorDepth == 16) ? 65535.0f : 255.0f;
        clTask ** tasks;
        clGammaErrorTermTask * infos;
        int tasksInFlight = 0;
        COLORIST_ASSERT(taskCount);

        printf("Using %d thread%s to find best gamma.\n", taskCount, (taskCount == 1) ? "" : "s");

        tasks = calloc(taskCount, sizeof(clTask *));
        infos = calloc(taskCount, sizeof(clGammaErrorTermTask));
        for (gammaInt = 20; gammaInt <= 80; ++gammaInt) { // (2.0 - 4.0) by 0.05
            float gammaAttempt = (float)gammaInt / 20.0f;

            infos[tasksInFlight].gammaInt = gammaInt;
            infos[tasksInFlight].gamma = gammaAttempt;
            infos[tasksInFlight].pixels = pixels;
            infos[tasksInFlight].pixelCount = pixelCount;
            infos[tasksInFlight].maxChannel = maxChannel;
            infos[tasksInFlight].luminanceScale = luminanceScale;
            infos[tasksInFlight].outErrorTerm = 0;
            tasks[tasksInFlight] = clTaskCreate((clTaskFunc)gammaErrorTermTaskFunc, &infos[tasksInFlight]);
            ++tasksInFlight;

            if ((tasksInFlight == taskCount) || (gammaInt == 50)) {
                for (i = 0; i < tasksInFlight; ++i) {
                    clTaskJoin(tasks[i]);
                    if (minErrorTerm < 0.0f) {
                        minErrorTerm = infos[i].outErrorTerm;
                        minGammaInt = infos[i].gammaInt;
                    } else if (minErrorTerm > infos[i].outErrorTerm) {
                        minErrorTerm = infos[i].outErrorTerm;
                        minGammaInt = infos[i].gammaInt;
                    }
                    if (verbose)
                        printf(" * [grading] gamma attempt (%g) error term: %g (best gamma is %g at error term %g)\n", infos[i].gamma, infos[i].outErrorTerm, (float)minGammaInt / 20.0f, minErrorTerm);
                    clTaskDestroy(tasks[i]);
                }
                tasksInFlight = 0;
            }
        }
        bestGamma = (float)minGammaInt / 20.0f;
        printf(" * [grading] Found best gamma: %g\n", bestGamma);
        free(tasks);
        free(infos);
    } else {
        bestGamma = *outGamma;
        printf(" * [grading] Using requested gamma: %g\n", bestGamma);
    }

    *outLuminance = maxLuminance;
    *outGamma = bestGamma;
}
