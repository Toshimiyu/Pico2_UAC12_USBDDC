#ifndef DIAGNOSTIC_STUCK_H_
#define DIAGNOSTIC_STUCK_H_

#include <stdbool.h>
#include <stdint.h>

static inline bool diagnostic_stuck_can_draw(bool slack, uint8_t stuck,
											 bool output_enabled, bool dma_silent)
{
	return !slack && stuck != 0u && !output_enabled && dma_silent;
}

static inline bool diagnostic_stuck_character_due(uint32_t now_us, uint32_t last_us)
{
	return (uint32_t)(now_us - last_us) >= 1000000u;
}

#endif
