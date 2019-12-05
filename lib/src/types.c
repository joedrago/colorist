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
#pragma warning(disable : 5031)
#pragma warning(disable : 5032)
#include <windows.h>
static uint64_t frequency;
static double now(void)
{
    if (!frequency) {
        LARGE_INTEGER perfFreq;
        QueryPerformanceFrequency(&perfFreq);
        frequency = perfFreq.QuadPart;
    }

    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    return (double)current.QuadPart / (double)frequency;
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
    data[0] = (s >> 8) & 0xff;
    data[1] = (s >> 0) & 0xff;
    uint16_t result;
    memcpy(&result, data, sizeof(uint16_t));
    return result;
}

uint16_t clNTOHS(uint16_t s)
{
    uint8_t data[2];
    memcpy(&data, &s, sizeof(data));

    return (uint16_t)((data[1] << 0) | (data[0] << 8));
}

uint32_t clHTONL(uint32_t l)
{
    uint8_t data[4];
    data[0] = (l >> 24) & 0xff;
    data[1] = (l >> 16) & 0xff;
    data[2] = (l >> 8) & 0xff;
    data[3] = (l >> 0) & 0xff;
    uint32_t result;
    memcpy(&result, data, sizeof(uint32_t));
    return result;
}

uint32_t clNTOHL(uint32_t l)
{
    uint8_t data[4];
    memcpy(&data, &l, sizeof(data));

    return ((uint32_t)data[3] << 0) | ((uint32_t)data[2] << 8) | ((uint32_t)data[1] << 16) | ((uint32_t)data[0] << 24);
}
