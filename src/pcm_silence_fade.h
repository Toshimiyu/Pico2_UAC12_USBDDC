#ifndef PCM_SILENCE_FADE_H_
#define PCM_SILENCE_FADE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint32_t silence_threshold_frames;
	uint32_t fade_frames;
	uint32_t silence_frames;
	uint32_t fade_remaining;
} pcm_silence_fade_t;

static inline uint32_t pcm_silence_fade_frame_count(uint32_t sample_rate, uint32_t duration_ms)
{
	const uint64_t frames = ((uint64_t)sample_rate * duration_ms + 999u) / 1000u;
	return frames == 0u ? 1u : (uint32_t)frames;
}

static inline void pcm_silence_fade_reset(pcm_silence_fade_t *fade, uint32_t sample_rate)
{
	fade->silence_threshold_frames = pcm_silence_fade_frame_count(sample_rate, 2u);
	fade->fade_frames = pcm_silence_fade_frame_count(sample_rate, 5u);
	fade->silence_frames = 0u;
	fade->fade_remaining = 0u;
}

static inline uint32_t pcm_silence_fade_next_gain(pcm_silence_fade_t *fade,
												  bool silent, bool dma_fades_in)
{
	if (silent)
	{
		if (fade->silence_frames < fade->silence_threshold_frames)
			fade->silence_frames++;
		if (fade->silence_frames >= fade->silence_threshold_frames)
			fade->fade_remaining = 0u;
		return 65536u;
	}

	const bool after_silence = fade->silence_frames >= fade->silence_threshold_frames;
	fade->silence_frames = 0u;
	if (dma_fades_in)
	{
		fade->fade_remaining = 0u;
		return 65536u;
	}
	if (after_silence)
		fade->fade_remaining = fade->fade_frames;

	if (fade->fade_remaining == 0u)
		return 65536u;

	const uint32_t elapsed = fade->fade_frames - fade->fade_remaining + 1u;
	const uint32_t gain = (uint32_t)(((uint64_t)elapsed << 16) / fade->fade_frames);
	fade->fade_remaining--;
	return gain;
}

#endif
