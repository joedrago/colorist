#include "colorist/transform.h"

#include "colorist/context.h"
#include "colorist/ccmm.h"
#include "colorist/profile.h"
#include "colorist/task.h"

static void doMultithreadedLCMSTransform(clContext * C, int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount);

clTransform * clTransformCreate(struct clContext * C, struct clProfile * srcProfile, clTransformFormat srcFormat, struct clProfile * dstProfile, clTransformFormat dstFormat)
{
    clTransform * transform = clAllocateStruct(clTransform);
    transform->srcProfile = srcProfile;
    transform->dstProfile = dstProfile;
    transform->srcFormat = srcFormat;
    transform->dstFormat = dstFormat;
    transform->ccmmReady = clFalse;
    transform->xyzProfile = NULL;
    transform->hTransform = NULL;
    return transform;
}

void clTransformDestroy(struct clContext * C, clTransform * transform)
{
    if (transform->hTransform) {
        cmsDeleteTransform(transform->hTransform);
    }
    if (transform->xyzProfile) {
        cmsCloseProfile(transform->xyzProfile);
    }
    clFree(transform);
}

static cmsUInt32Number clTransformFormatToLCMSFormat(struct clContext * C, clTransformFormat format)
{
    switch (format) {
        case CL_TF_XYZ_FLOAT:  return TYPE_XYZ_FLT;
        case CL_TF_RGB_FLOAT:  return TYPE_RGB_FLT;
        case CL_TF_RGBA_FLOAT: return TYPE_RGBA_FLT;
        case CL_TF_RGBA_8:     return TYPE_RGBA_8;
        case CL_TF_RGBA_16:    return TYPE_RGBA_16;
    }

    COLORIST_FAILURE("clTransformFormatToLCMSFormat: Unknown transform format");
    return TYPE_RGBA_FLT;
}

int clTransformFormatToPixelBytes(struct clContext * C, clTransformFormat format)
{
    switch (format) {
        case CL_TF_XYZ_FLOAT:  return sizeof(float) * 3;
        case CL_TF_RGB_FLOAT:  return sizeof(float) * 3;
        case CL_TF_RGBA_FLOAT: return sizeof(float) * 4;
        case CL_TF_RGBA_8:     4;
        case CL_TF_RGBA_16:    8;
    }

    COLORIST_FAILURE("clTransformFormatToPixelBytes: Unknown transform format");
    return sizeof(float) * 4;
}

void clTransformRun(struct clContext * C, clTransform * transform, int taskCount, void * srcPixels, void * dstPixels, int pixelCount)
{
    clBool useCCMM = C->ccmmAllowed;
    if (transform->srcProfile && !transform->srcProfile->ccmm) {
        useCCMM = clFalse;
    }
    if (transform->dstProfile && !transform->dstProfile->ccmm) {
        useCCMM = clFalse;
    }

    if (useCCMM) {
        // Use colorist CMM
        clCCMMTransform(C, transform, taskCount, srcPixels, dstPixels, pixelCount);
    } else {
        // Use LittleCMS
        int srcPixelBytes = clTransformFormatToPixelBytes(C, transform->srcFormat);
        int dstPixelBytes = clTransformFormatToPixelBytes(C, transform->dstFormat);
        if (!transform->hTransform) {
            cmsUInt32Number srcFormat = clTransformFormatToLCMSFormat(C, transform->srcFormat);
            cmsUInt32Number dstFormat = clTransformFormatToLCMSFormat(C, transform->dstFormat);
            cmsHPROFILE srcProfileHandle;
            cmsHPROFILE dstProfileHandle;

            // Choose src profile handle
            if (transform->srcProfile) {
                srcProfileHandle = transform->srcProfile->handle;
            } else {
                if (!transform->xyzProfile) {
                    transform->xyzProfile = cmsCreateXYZProfileTHR(C->lcms);
                }
                srcProfileHandle = transform->xyzProfile;
            }

            // Choose dst profile handle
            if (transform->dstProfile) {
                dstProfileHandle = transform->dstProfile->handle;
            } else {
                if (!transform->xyzProfile) {
                    transform->xyzProfile = cmsCreateXYZProfileTHR(C->lcms);
                }
                dstProfileHandle = transform->xyzProfile;
            }

            // Lazily create hTransform
            transform->hTransform = cmsCreateTransformTHR(C->lcms, srcProfileHandle, srcFormat, dstProfileHandle, dstFormat, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_NOOPTIMIZE);
        }
        doMultithreadedLCMSTransform(C, taskCount, transform->hTransform, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
    }

}

typedef struct clTransformTask
{
    cmsHTRANSFORM transform;
    void * inPixels;
    void * outPixels;
    int pixelCount;
} clTransformTask;

static void lcmsTransformTaskFunc(clTransformTask * info)
{
    cmsDoTransform(info->transform, info->inPixels, info->outPixels, info->pixelCount);
}

static void doMultithreadedLCMSTransform(clContext * C, int taskCount, cmsHTRANSFORM transform, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    if (taskCount > pixelCount) {
        // This is a dumb corner case I'm not too worried about.
        taskCount = pixelCount;
    }

    if (taskCount == 1) {
        // Don't bother making any new threads
        cmsDoTransform(transform, srcPixels, dstPixels, pixelCount);
    } else {
        int pixelsPerTask = pixelCount / taskCount;
        int lastTaskPixelCount = pixelCount - (pixelsPerTask * (taskCount - 1));
        clTask ** tasks;
        clTransformTask * infos;
        int i;

        clContextLog(C, "convert", 1, "Using %d thread%s to pixel transform.", taskCount, (taskCount == 1) ? "" : "s");

        tasks = clAllocate(taskCount * sizeof(clTask *));
        infos = clAllocate(taskCount * sizeof(clTransformTask));
        for (i = 0; i < taskCount; ++i) {
            infos[i].transform = transform;
            infos[i].inPixels = &srcPixels[i * pixelsPerTask * srcPixelBytes];
            infos[i].outPixels = &dstPixels[i * pixelsPerTask * dstPixelBytes];
            infos[i].pixelCount = (i == (taskCount - 1)) ? lastTaskPixelCount : pixelsPerTask;
            tasks[i] = clTaskCreate(C, (clTaskFunc)lcmsTransformTaskFunc, &infos[i]);
        }

        for (i = 0; i < taskCount; ++i) {
            clTaskDestroy(C, tasks[i]);
        }

        clFree(tasks);
        clFree(infos);
    }
}
