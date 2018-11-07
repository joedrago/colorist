// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/task.h"

#include "colorist/context.h"

static void nativeTaskStart(clContext * C, clTask * task);
static void nativeTaskJoin(clContext * C, clTask * task);

clTask * clTaskCreate(struct clContext * C, clTaskFunc func, void * userData)
{
    clTask * task = clAllocateStruct(clTask);
    task->func = func;
    task->nativeData = NULL;
    task->userData = userData;
    task->joined = clFalse;
    nativeTaskStart(C, task);
    return task;
}

void clTaskJoin(struct clContext * C, clTask * task)
{
    if (!task->joined) {
        nativeTaskJoin(C, task);
        task->joined = clTrue;
    }
}

void clTaskDestroy(struct clContext * C, clTask * task)
{
    clTaskJoin(C, task);
    COLORIST_ASSERT(task->joined);
    COLORIST_ASSERT(task->nativeData == NULL);
    clFree(task);
}

#ifdef _WIN32

#include <windows.h>

int clTaskLimit(void)
{
    int numCPU;
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    numCPU = sysinfo.dwNumberOfProcessors;
    return numCPU;
}

typedef struct clNativeTask
{
    HANDLE hThread;
} clNativeTask;

static DWORD WINAPI taskThreadProc(LPVOID lpParameter)
{
    clTask * task = (clTask *)lpParameter;
    task->func(task->userData);
    return 0;
}

static void nativeTaskStart(clContext * C, clTask * task)
{
    DWORD threadId;
    clNativeTask * nativeTask = clAllocateStruct(clNativeTask);
    task->nativeData = nativeTask;
    nativeTask->hThread = CreateThread(NULL, 0, taskThreadProc, task, 0, &threadId);
}

static void nativeTaskJoin(clContext * C, clTask * task)
{
    clNativeTask * nativeTask = (clNativeTask *)task->nativeData;
    WaitForSingleObject(nativeTask->hThread, INFINITE);
    CloseHandle(nativeTask->hThread);
    clFree(task->nativeData);
    task->nativeData = NULL;
}

#else /* ifdef _WIN32 */

#ifdef __APPLE__

#include <sys/sysctl.h>

int clTaskLimit()
{
    int mib[4];
    int numCPU;
    size_t len = sizeof(numCPU);

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU; // alternatively, try HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &numCPU, &len, NULL, 0);

    if (numCPU < 1) {
        mib[1] = HW_NCPU;
        sysctl(mib, 2, &numCPU, &len, NULL, 0);
        if (numCPU < 1)
            numCPU = 1;
    }
    return numCPU;
}
#else

#include <unistd.h>

int clTaskLimit()
{
#ifdef COLORIST_EMSCRIPTEN
    return 1;
#else
    int numCPU = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return CL_CLAMP(numCPU, 1, numCPU);
#endif
}
#endif /* ifdef __APPLE__ */

#include <pthread.h>

typedef struct clNativeTask
{
    pthread_t pthread;
} clNativeTask;

static void * taskThreadProc(void * userData)
{
    clTask * task = (clTask *)userData;
    task->func(task->userData);
    pthread_exit(NULL);
}

static void nativeTaskStart(clContext * C, clTask * task)
{
    clNativeTask * nativeTask = clAllocateStruct(clNativeTask);
    task->nativeData = nativeTask;
    pthread_create(&nativeTask->pthread, NULL, taskThreadProc, task);
}

static void nativeTaskJoin(clContext * C, clTask * task)
{
    clNativeTask * nativeTask = (clNativeTask *)task->nativeData;
    pthread_join(nativeTask->pthread, NULL);
    clFree(task->nativeData);
    task->nativeData = NULL;
}

#endif /* ifdef _WIN32 */
