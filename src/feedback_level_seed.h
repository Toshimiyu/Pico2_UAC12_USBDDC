#ifndef FEEDBACK_LEVEL_SEED_H
#define FEEDBACK_LEVEL_SEED_H

#include <stdbool.h>

static inline float feedback_level_seed_update(float filtered, float level, float alpha,
	bool *pending, bool dma_primed)
{
	if (*pending && dma_primed)
	{
		*pending = false;
		return level;
	}
	return filtered + (level - filtered) * alpha;
}

#endif
