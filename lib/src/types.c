// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/types.h"

#include <string.h>

static double now(void);

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
static double now(void)
{
    return (double)GetTickCount64() / 1000.0;
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

// Thanks, Rob Pike! https://commandcenter.blogspot.nl/2012/04/byte-order-fallacy.html

uint16_t clHTONS(uint16_t s)
{
    uint8_t data[2];
    memcpy(&data, &s, sizeof(data));

    return (uint16_t)((data[0] << 0)
                      | (data[1] << 8));
}

uint16_t clNTOHS(uint16_t s)
{
    uint8_t data[2];
    memcpy(&data, &s, sizeof(data));

    return (uint16_t)((data[1] << 0)
                      | (data[0] << 8));
}

uint32_t clHTONL(uint32_t l)
{
    uint8_t data[4];
    memcpy(&data, &l, sizeof(data));

    return ((uint32_t)data[0] << 0)
           | ((uint32_t)data[1] << 8)
           | ((uint32_t)data[2] << 16)
           | ((uint32_t)data[3] << 24);
}

uint32_t clNTOHL(uint32_t l)
{
    uint8_t data[4];
    memcpy(&data, &l, sizeof(data));

    return ((uint32_t)data[3] << 0)
           | ((uint32_t)data[2] << 8)
           | ((uint32_t)data[1] << 16)
           | ((uint32_t)data[0] << 24);
}
