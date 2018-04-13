// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/task.h"

#include <stdlib.h>

void * clContextDefaultAlloc(struct clContext * C, int bytes)
{
    return calloc(1, bytes);
}

void clContextDefaultFree(struct clContext * C, void * ptr)
{
    free(ptr);
}
