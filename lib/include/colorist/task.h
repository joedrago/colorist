// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_TASK_H
#define COLORIST_TASK_H

#include "colorist/types.h"

struct clContext;

typedef void (* clTaskFunc)(void * userData);

typedef struct clTask
{
    clTaskFunc func;
    void * nativeData;
    void * userData;
    clBool joined;
} clTask;

clTask * clTaskCreate(struct clContext * C, clTaskFunc func, void * userData);
void clTaskJoin(struct clContext * C, clTask * task);
void clTaskDestroy(struct clContext * C, clTask * task);
int clTaskLimit(void);

#endif // ifndef COLORIST_TASK_H
