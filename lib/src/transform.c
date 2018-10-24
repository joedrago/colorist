#include "colorist/transform.h"

#include "colorist/context.h"
#include "colorist/ccmm.h"
#include "colorist/profile.h"
#include "colorist/task.h"

static void doMultithreadedTransform(clContext * C, int taskCount, clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount);

void clTransformXYZToXYY(struct clContext * C, float * dstXYY, float * srcXYZ, float whitePointX, float whitePointY)
{
    float sum = srcXYZ[0] + srcXYZ[1] + srcXYZ[2];
    if (sum <= 0.0f) {
        dstXYY[0] = whitePointX;
        dstXYY[1] = whitePointY;
        dstXYY[2] = 0.0f;
        return;
    }
    dstXYY[0] = srcXYZ[0] / sum;
    dstXYY[1] = srcXYZ[1] / sum;
    dstXYY[2] = srcXYZ[1];
}

void clTransformXYYToXYZ(struct clContext * C, float * dstXYZ, float * srcXYY)
{
    if (srcXYY[2] <= 0.0f) {
        dstXYZ[0] = 0.0f;
        dstXYZ[1] = 0.0f;
        dstXYZ[2] = 0.0f;
        return;
    }
    dstXYZ[0] = (srcXYY[0] * srcXYY[2]) / srcXYY[1];
    dstXYZ[1] = srcXYY[2];
    dstXYZ[2] = ((1 - srcXYY[0] - srcXYY[1]) * srcXYY[2]) / srcXYY[1];
}

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
        case CL_TF_RGB_8:     return TYPE_RGB_8;
        case CL_TF_RGBA_8:     return TYPE_RGBA_8;
        case CL_TF_RGB_16:    return TYPE_RGB_16;
        case CL_TF_RGBA_16:    return TYPE_RGBA_16;
    }

    COLORIST_FAILURE("clTransformFormatToLCMSFormat: Unknown transform format");
    return TYPE_RGBA_FLT;
}

clBool clTransformFormatIsFloat(struct clContext * C, clTransformFormat format)
{
    switch (format) {
        case CL_TF_XYZ_FLOAT:
        case CL_TF_RGB_FLOAT:
        case CL_TF_RGBA_FLOAT:
            return clTrue;

        case CL_TF_RGB_8:
        case CL_TF_RGBA_8:
        case CL_TF_RGB_16:
        case CL_TF_RGBA_16:
            break;
    }
    return clFalse;
}

int clTransformFormatToPixelBytes(struct clContext * C, clTransformFormat format)
{
    switch (format) {
        case CL_TF_XYZ_FLOAT:  return sizeof(float) * 3;
        case CL_TF_RGB_FLOAT:  return sizeof(float) * 3;
        case CL_TF_RGBA_FLOAT: return sizeof(float) * 4;
        case CL_TF_RGB_8:      return sizeof(uint8_t) * 3;
        case CL_TF_RGBA_8:     return sizeof(uint8_t) * 4;
        case CL_TF_RGB_16:     return sizeof(uint16_t) * 3;
        case CL_TF_RGBA_16:    return sizeof(uint16_t) * 4;
    }

    COLORIST_FAILURE("clTransformFormatToPixelBytes: Unknown transform format");
    return sizeof(float) * 4;
}

clBool clTransformUsesCCMM(struct clContext * C, clTransform * transform)
{
    clBool useCCMM = C->ccmmAllowed;
    if (transform->srcProfile && !transform->srcProfile->ccmm) {
        useCCMM = clFalse;
    }
    if (transform->dstProfile && !transform->dstProfile->ccmm) {
        useCCMM = clFalse;
    }
    return useCCMM;
}

void clTransformRun(struct clContext * C, clTransform * transform, int taskCount, void * srcPixels, void * dstPixels, int pixelCount)
{
    int srcPixelBytes = clTransformFormatToPixelBytes(C, transform->srcFormat);
    int dstPixelBytes = clTransformFormatToPixelBytes(C, transform->dstFormat);

    clBool useCCMM = clTransformUsesCCMM(C, transform);

    if (taskCount > 1) {
        clContextLog(C, "convert", 1, "Using %d threads to pixel transform. (via %s)", taskCount, useCCMM ? "CCMM" : "LittleCMS");
    }

    if (useCCMM) {
        // Use colorist CMM
        clCCMMPrepareTransform(C, transform);
    } else {
        // Use LittleCMS
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
    }
    doMultithreadedTransform(C, taskCount, transform, useCCMM, srcPixels, srcPixelBytes, dstPixels, dstPixelBytes, pixelCount);
}

typedef struct clTransformTask
{
    clContext * C;
    clTransform * transform;
    void * inPixels;
    void * outPixels;
    int pixelCount;
    clBool useCCMM;
} clTransformTask;

static void transformTaskFunc(clTransformTask * info)
{
    if (info->useCCMM)
        clCCMMTransform(info->C, info->transform, info->inPixels, info->outPixels, info->pixelCount);
    else
        cmsDoTransform(info->transform->hTransform, info->inPixels, info->outPixels, info->pixelCount);
}

static void doMultithreadedTransform(clContext * C, int taskCount, clTransform * transform, clBool useCCMM, uint8_t * srcPixels, int srcPixelBytes, uint8_t * dstPixels, int dstPixelBytes, int pixelCount)
{
    if (taskCount > pixelCount) {
        // This is a dumb corner case I'm not too worried about.
        taskCount = pixelCount;
    }

    if (taskCount == 1) {
        // Don't bother making any new threads
        clTransformTask info;
        info.C = C;
        info.transform = transform;
        info.inPixels = srcPixels;
        info.outPixels = dstPixels;
        info.pixelCount = pixelCount;
        info.useCCMM = useCCMM;
        transformTaskFunc(&info);
    } else {
        int pixelsPerTask = pixelCount / taskCount;
        int lastTaskPixelCount = pixelCount - (pixelsPerTask * (taskCount - 1));
        clTask ** tasks;
        clTransformTask * infos;
        int i;

        tasks = clAllocate(taskCount * sizeof(clTask *));
        infos = clAllocate(taskCount * sizeof(clTransformTask));
        for (i = 0; i < taskCount; ++i) {
            infos[i].C = C;
            infos[i].transform = transform;
            infos[i].inPixels = &srcPixels[i * pixelsPerTask * srcPixelBytes];
            infos[i].outPixels = &dstPixels[i * pixelsPerTask * dstPixelBytes];
            infos[i].pixelCount = (i == (taskCount - 1)) ? lastTaskPixelCount : pixelsPerTask;
            infos[i].useCCMM = useCCMM;
            tasks[i] = clTaskCreate(C, (clTaskFunc)transformTaskFunc, &infos[i]);
        }

        for (i = 0; i < taskCount; ++i) {
            clTaskDestroy(C, tasks[i]);
        }

        clFree(tasks);
        clFree(infos);
    }
}
