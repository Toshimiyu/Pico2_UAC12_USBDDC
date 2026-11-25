#ifndef AUDIO_FADE_H_
#define AUDIO_FADE_H_

#include <stdint.h>

static inline uint32_t audio_fade_gain_q16(uint32_t numerator, uint32_t total)
{
	return (numerator << 16) / total;
}

static inline int32_t audio_fade_apply_q16(int32_t sample, uint32_t gain)
{
	const int64_t product = (int64_t)sample * (int32_t)gain;
	if (product < 0)
		return (int32_t)(-((int64_t)((uint64_t)(-product) >> 16)));
	return (int32_t)((uint64_t)product >> 16);
}

static inline int32_t audio_fade_apply_q6(int32_t sample, uint32_t numerator)
{
	const int64_t product = (int64_t)sample * numerator;
	if (product < 0)
		return (int32_t)(-((int64_t)((uint64_t)(-product) >> 6)));
	return (int32_t)((uint64_t)product >> 6);
}

#endif
