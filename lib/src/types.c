// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/types.h"

static double now();

void timerStart(Timer * timer)
{
    timer->start = now();
}

double timerElapsedSeconds(Timer * timer)
{
    return now() - timer->start;
}

#ifdef _WIN32
#include <windows.h>
static double now()
{
    return (double)GetTickCount() / 1000.0;
}
#else
#include <sys/time.h>
static double now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
}
#endif
