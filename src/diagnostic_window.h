#ifndef DIAGNOSTIC_WINDOW_H
#define DIAGNOSTIC_WINDOW_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint32_t start_us;
	uint32_t min;
	uint32_t max;
	uint32_t last_min;
	uint32_t last_max;
	bool active;
	bool valid;
} diagnostic_window_t;

static inline void diagnostic_window_sample(diagnostic_window_t *window,
	bool streaming, uint32_t now_us, uint32_t level)
{
	if (!streaming)
	{
		*window = (diagnostic_window_t){0};
		return;
	}
	if (!window->active)
	{
		window->active = true;
		window->start_us = now_us;
		window->min = window->max = level;
		return;
	}
	if (level < window->min)
		window->min = level;
	if (level > window->max)
		window->max = level;
	if ((uint32_t)(now_us - window->start_us) >= 1000000u)
	{
		window->last_min = window->min;
		window->last_max = window->max;
		window->valid = true;
		window->start_us = now_us;
		window->min = window->max = level;
	}
}

#endif
