#include "colorist/pixelmath.h"

#include "colorist/context.h"
#include "colorist/task.h"

#include <math.h>

// (1.0 - 4.0) by 0.05
#define GAMMA_RANGE_START 20
#define GAMMA_RANGE_END 80
#define GAMMA_INT_DIVISOR 20.0f

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
    float channelErrorTerm;
    float scaledChannel;
    float * pixel = pixels;
    int i;

    for (i = 0; i < pixelCount; ++i) {
        scaledChannel = pixel[0] * luminanceScale;
        scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
        channelErrorTerm = fabsf(scaledChannel - powf(clPixelMathRoundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gamma));
        errorTerm += channelErrorTerm; // * channelErrorTerm;

        scaledChannel = pixel[1] * luminanceScale;
        scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
        channelErrorTerm = fabsf(scaledChannel - powf(clPixelMathRoundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gamma));
        errorTerm += channelErrorTerm; // * channelErrorTerm;

        scaledChannel = pixel[2] * luminanceScale;
        scaledChannel = CL_CLAMP(scaledChannel, 0.0f, 1.0f);
        channelErrorTerm = fabsf(scaledChannel - powf(clPixelMathRoundf(powf(scaledChannel, invGamma) * maxChannel) / maxChannel, gamma));
        errorTerm += channelErrorTerm; // * channelErrorTerm;

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

void clPixelMathColorGrade(struct clContext * C, int taskCount, float * pixels, int pixelCount, int srcLuminance, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose)
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
        clContextLog(C, "grading", 1, "Found max luminance: %d nits", maxLuminance);
    } else {
        maxLuminance = *outLuminance;
        clContextLog(C, "grading", 1, "Using requested max luminance: %d nits", maxLuminance);
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

        clContextLog(C, "grading", 1, "Using %d thread%s to find best gamma.", taskCount, (taskCount == 1) ? "" : "s");

        tasks = clAllocate(taskCount * sizeof(clTask *));
        infos = clAllocate(taskCount * sizeof(clGammaErrorTermTask));
        for (gammaInt = GAMMA_RANGE_START; gammaInt <= GAMMA_RANGE_END; ++gammaInt) {
            float gammaAttempt = (float)gammaInt / GAMMA_INT_DIVISOR;

            infos[tasksInFlight].gammaInt = gammaInt;
            infos[tasksInFlight].gamma = gammaAttempt;
            infos[tasksInFlight].pixels = pixels;
            infos[tasksInFlight].pixelCount = pixelCount;
            infos[tasksInFlight].maxChannel = maxChannel;
            infos[tasksInFlight].luminanceScale = luminanceScale;
            infos[tasksInFlight].outErrorTerm = 0;
            tasks[tasksInFlight] = clTaskCreate(C, (clTaskFunc)gammaErrorTermTaskFunc, &infos[tasksInFlight]);
            ++tasksInFlight;

            if ((tasksInFlight == taskCount) || (gammaInt == GAMMA_RANGE_END)) {
                for (i = 0; i < tasksInFlight; ++i) {
                    clTaskJoin(C, tasks[i]);
                    if (minErrorTerm < 0.0f) {
                        minErrorTerm = infos[i].outErrorTerm;
                        minGammaInt = infos[i].gammaInt;
                    } else if (minErrorTerm > infos[i].outErrorTerm) {
                        minErrorTerm = infos[i].outErrorTerm;
                        minGammaInt = infos[i].gammaInt;
                    }
                    if (verbose)
                        clContextLog(C, "grading", 2, "attempt: gamma %.3g, err: %g     best -> gamma: %g, err: %g", infos[i].gamma, infos[i].outErrorTerm, (float)minGammaInt / GAMMA_INT_DIVISOR, minErrorTerm);
                    clTaskDestroy(C, tasks[i]);
                }
                tasksInFlight = 0;
            }
        }
        bestGamma = (float)minGammaInt / GAMMA_INT_DIVISOR;
        clContextLog(C, "grading", 1, "Found best gamma: %g", bestGamma);
        clFree(tasks);
        clFree(infos);
    } else {
        bestGamma = *outGamma;
        clContextLog(C, "grading", 1, "Using requested gamma: %g", bestGamma);
    }

    *outLuminance = maxLuminance;
    *outGamma = bestGamma;
}
