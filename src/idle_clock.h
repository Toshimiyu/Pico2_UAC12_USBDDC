#ifndef IDLE_CLOCK_H_
#define IDLE_CLOCK_H_

#include <stdbool.h>
#include <stdint.h>

#define IDLE_CLOCK_TIMEOUT_US (1000000u)

typedef struct
{
	uint32_t sequence;
	uint32_t last_activity_us;
	bool initialized;
} idle_clock_timer_t;

static inline bool idle_clock_expired(idle_clock_timer_t *timer, uint32_t now_us,
									  uint32_t sequence, bool inhibited)
{
	if (!timer->initialized || timer->sequence != sequence || inhibited)
	{
		timer->initialized = true;
		timer->sequence = sequence;
		timer->last_activity_us = now_us;
		return false;
	}
	return (uint32_t)(now_us - timer->last_activity_us) >= IDLE_CLOCK_TIMEOUT_US;
}

#endif
