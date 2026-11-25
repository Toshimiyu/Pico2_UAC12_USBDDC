#ifndef STOP_FADE_H_
#define STOP_FADE_H_

#include "mute_ramp.h"
#include "audio_fade.h"
#include <string.h>

#define STOP_FADE_MS 20u

static inline void stop_fade_reset(mute_ramp_t *ramp, uint32_t sample_rate)
{
	const uint32_t whole_thousands = sample_rate / 1000u;
	const uint32_t remainder = sample_rate % 1000u;
	ramp->frames = whole_thousands * STOP_FADE_MS +
		(remainder * STOP_FADE_MS + 999u) / 1000u;
	if (ramp->frames == 0u)
		ramp->frames = 1u;
	ramp->position = ramp->frames;
}

static inline void stop_fade_apply(mute_ramp_t *ramp, int32_t *samples, uint32_t frames)
{
	for (uint32_t i = 0; i < frames; i++)
	{
		const uint32_t gain = mute_ramp_next(ramp, true);
		if (gain == 0u)
		{
			memset(&samples[2u * i], 0, (frames - i) * 2u * sizeof(*samples));
			break;
		}
		const uint32_t gain_q16 = audio_fade_gain_q16(gain, ramp->frames);
		samples[2u * i] = audio_fade_apply_q16(samples[2u * i], gain_q16);
		samples[2u * i + 1u] = audio_fade_apply_q16(samples[2u * i + 1u], gain_q16);
	}
}

#endif
