#ifndef HOST_RATE_WINDOW_H
#define HOST_RATE_WINDOW_H

#include <stdbool.h>
#include <stdint.h>

#define HOST_RATE_UPDATE_US 250000u

typedef struct
{
    uint32_t time_us;
    uint32_t frames;
} host_rate_window_t;

static inline bool host_rate_window_update(host_rate_window_t *window, uint32_t now,
                                           uint32_t frames, bool active,
                                           uint32_t *rate)
{
    if (!active)
    {
        window->time_us = now;
        window->frames = frames;
        *rate = 0;
        return false;
    }

    const uint32_t dt = now - window->time_us;
    if (dt < HOST_RATE_UPDATE_US)
        return false;

    *rate = (uint32_t)(((uint64_t)(frames - window->frames) * 1000000ull + dt / 2u) / dt);
    window->time_us = now;
    window->frames = frames;
    return true;
}

#endif
