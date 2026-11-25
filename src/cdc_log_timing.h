#ifndef CDC_LOG_TIMING_H
#define CDC_LOG_TIMING_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
	uint32_t previous_us;
	uint32_t max_gap_us;
	uint32_t over_1ms;
	uint32_t state_at_max;
} cdc_task_gap_window_t;

static inline void cdc_task_gap_record(cdc_task_gap_window_t *window, uint32_t now_us,
									   uint32_t state)
{
	const uint32_t gap = (uint32_t)(now_us - window->previous_us);
	window->previous_us = now_us;
	if (gap > window->max_gap_us)
	{
		window->max_gap_us = gap;
		window->state_at_max = state;
	}
	if (gap > 1000u)
		window->over_1ms++;
}

static inline void cdc_task_gap_clear(cdc_task_gap_window_t *window)
{
	window->max_gap_us = 0;
	window->over_1ms = 0;
}

static inline bool cdc_fast_sample_due(uint32_t now, uint32_t *next_us, uint32_t *missed)
{
	if ((int32_t)(now - *next_us) < 0)
		return false;
	const uint32_t elapsed_periods = (now - *next_us) / 1000u;
	*missed += elapsed_periods;
	*next_us += (elapsed_periods + 1u) * 1000u;
	return true;
}

static inline bool cdc_parse_time_sync(const char *line, uint64_t *unix_us)
{
	if (strncmp(line, "TIME,", 5) != 0 || line[5] == '\0')
		return false;

	uint64_t value = 0;
	for (const char *p = line + 5; *p != '\0'; ++p)
	{
		if (*p < '0' || *p > '9')
			return false;
		const uint32_t digit = (uint32_t)(*p - '0');
		if (value > (UINT64_MAX - digit) / 10u)
			return false;
		value = value * 10u + digit;
	}
	// Leave room for uptime and reject dates outside the civil calendar.
	if (value == 0 || value > 253402300799999999ull)
		return false;
	*unix_us = value;
	return true;
}

typedef enum
{
	CDC_COMMAND_INVALID,
	CDC_COMMAND_TIME,
	CDC_COMMAND_LOG_START
} cdc_command_t;

static inline cdc_command_t cdc_parse_command(const char *line, uint64_t *unix_us)
{
	if (strcmp(line, "LOG_START") == 0)
		return CDC_COMMAND_LOG_START;
	if (cdc_parse_time_sync(line, unix_us))
		return CDC_COMMAND_TIME;
	return CDC_COMMAND_INVALID;
}

#endif
