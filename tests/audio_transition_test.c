#include <stdint.h>

#include "audio_fade.h"
#include "audio_wait_timeout.h"
#include "diagnostic_stuck.h"
#include "pcm_silence_fade.h"

static int test_pcm_silence_fade(void)
{
	pcm_silence_fade_t fade;
	pcm_silence_fade_reset(&fade, 48000u);
	for (uint32_t i = 0u; i < fade.silence_threshold_frames; i++)
	{
		if (pcm_silence_fade_next_gain(&fade, true, false) != 65536u)
			return 18;
	}

	const uint32_t first_gain = pcm_silence_fade_next_gain(&fade, false, false);
	if (first_gain == 0u || first_gain >= 65536u ||
		fade.fade_remaining != fade.fade_frames - 1u)
		return 19;
	for (uint32_t i = 1u; i < fade.fade_frames; i++)
	{
		if (pcm_silence_fade_next_gain(&fade, false, false) !=
			(uint32_t)(((uint64_t)(i + 1u) << 16) / fade.fade_frames))
			return 20;
	}
	if (pcm_silence_fade_next_gain(&fade, false, false) != 65536u)
		return 21;

	pcm_silence_fade_reset(&fade, 48000u);
	for (uint32_t i = 0u; i < fade.silence_threshold_frames; i++)
		(void)pcm_silence_fade_next_gain(&fade, true, false);
	if (pcm_silence_fade_next_gain(&fade, false, true) != 65536u ||
		pcm_silence_fade_next_gain(&fade, false, false) != 65536u)
		return 22;
	return 0;
}

static int test_fade(void)
{
	const uint32_t total = 960u;
	if (audio_fade_gain_q16(1u, total) == 0u ||
		audio_fade_gain_q16(total, total) != 65536u)
		return 1;

	const uint32_t gain = audio_fade_gain_q16(480u, total);
	if (audio_fade_apply_q16(2000000000, gain) != 1000000000 ||
		audio_fade_apply_q16(-2000000000, gain) >= 0 ||
		audio_fade_apply_q6(-6400, 32u) != -3200)
		return 2;
	return 0;
}

static int test_fade_symmetry(void)
{
	const int32_t boundaries[] = {0, 1, -1, 2, -2, INT32_MAX, INT32_MIN};
	for (uint32_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++)
	{
		if (audio_fade_apply_q16(boundaries[i], 0u) != 0 ||
			audio_fade_apply_q16(boundaries[i], 65536u) != boundaries[i])
			return 6;
	}
	if (audio_fade_apply_q6(INT32_MIN, 64u) != INT32_MIN)
		return 7;

	const int32_t samples[] = {1, 2, 32767, 882000, 2000000000, INT32_MAX};
	const uint32_t gains[] = {0u, 1u, 32768u, 65535u, 65536u};
	for (uint32_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
	{
		for (uint32_t j = 0; j < sizeof(gains) / sizeof(gains[0]); j++)
		{
			const int32_t positive = audio_fade_apply_q16(samples[i], gains[j]);
			const int32_t negative = audio_fade_apply_q16(-samples[i], gains[j]);
			if (positive != -negative)
				return 5;
		}
	}
	return 0;
}

static int test_timeout(void)
{
	audio_wait_timeout_t timer = {0};
	if (audio_wait_timeout_update(&timer, true, UINT32_MAX - 49u, 100u) ||
		audio_wait_timeout_update(&timer, true, 49u, 100u) ||
		!audio_wait_timeout_update(&timer, true, 50u, 100u) ||
		audio_wait_timeout_update(&timer, true, 150u, 100u))
		return 3;
	if (audio_wait_timeout_update(&timer, false, 151u, 100u) ||
		audio_wait_timeout_update(&timer, true, 200u, 100u) ||
		!audio_wait_timeout_update(&timer, true, 300u, 100u))
		return 4;
	return 0;
}

static int test_stuck_drawing(void)
{
	uint32_t draws = 0u;
	uint32_t last_draw = 0u;
	audio_wait_timeout_t buffering = {0};

	const uint32_t rates[] = {44100u, 48000u, 88200u, 96000u, 176400u, 192000u};
	for (uint32_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++)
	{
		/* Model normal prefill of the configured 3072 frames. */
		const uint32_t prefill_ms = (3072000u + rates[i] - 1u) / rates[i];
		for (uint32_t ms = 0u; ms <= prefill_ms; ms++)
		{
			const bool stuck = audio_wait_elapsed(true, ms * 1000u, 0u, 1000000u);
			if (diagnostic_stuck_can_draw(false, stuck, false, true))
				draws++;
		}
		if (draws != 0u ||
			audio_wait_elapsed(true, 999000u, 0u, 1000000u) ||
			!audio_wait_elapsed(true, 1000000u, 0u, 1000000u) ||
			audio_wait_elapsed(true, 249000u, 0u, 250000u) ||
			!audio_wait_elapsed(true, 250000u, 0u, 250000u))
			return 8;
	}
	if (audio_wait_elapsed(true, 999999u, 0u, 1000000u) ||
		!audio_wait_elapsed(true, 1000000u, 0u, 1000000u))
		return 8;

	/* Simulate a wait started two seconds ago, then stop it mid-line. */
	for (uint32_t now = 2000000u; now <= 4000000u; now += 100000u)
	{
		const bool waiting = now < 3200000u;
		const bool stuck = audio_wait_elapsed(waiting, now, 0u, 1000000u);
		if (diagnostic_stuck_can_draw(false, stuck, false, true) &&
			diagnostic_stuck_character_due(now, last_draw))
		{
			draws++;
			last_draw = now;
		}
	}
	if (draws != 2u ||
		diagnostic_stuck_can_draw(true, 1u, false, true) ||
		diagnostic_stuck_can_draw(false, 1u, true, true) ||
		diagnostic_stuck_can_draw(false, 1u, false, false) ||
		diagnostic_stuck_can_draw(false, 0u, false, true))
		return 9;

	/* Leaving a wait, a fresh entry, clock readiness and timer wrap. */
	if (audio_wait_elapsed(false, 2000000u, 0u, 1000000u) ||
		audio_wait_elapsed(true, 2001000u, 2000000u, 1000000u) ||
		audio_wait_elapsed(true, 249999u, 0u, 250000u) ||
		!audio_wait_elapsed(true, 250000u, 0u, 250000u) ||
		audio_wait_elapsed(false, 250000u, 0u, 250000u) ||
		!audio_wait_elapsed(true, 49u, UINT32_MAX - 50u, 100u))
		return 10;

	/* A long idle must not reuse an old BUFFERING entry time on resume. */
	(void)audio_wait_timeout_update(&buffering, true, 0u, 1000000u);
	(void)audio_wait_timeout_update(&buffering, false, 50000u, 1000000u);
	if (audio_wait_buffering_stuck(true, false, &buffering, 60000000u, 0u, 1000000u))
		return 11;
	(void)audio_wait_timeout_update(&buffering, true, 60000000u, 1000000u);
	if (audio_wait_buffering_stuck(true, true, &buffering, 60046000u, 0u, 1000000u) ||
		audio_wait_buffering_stuck(true, true, &buffering, 60999000u, 0u, 1000000u) ||
		!audio_wait_buffering_stuck(true, true, &buffering, 61000000u, 0u, 1000000u) ||
		audio_wait_buffering_stuck(false, true, &buffering, 61000000u, 0u, 1000000u))
		return 12;
	return 0;
}

int main(void)
{
	const int silence_fade_result = test_pcm_silence_fade();
	if (silence_fade_result != 0)
		return silence_fade_result;
	const int fade_result = test_fade();
	if (fade_result != 0)
		return fade_result;
	const int symmetry_result = test_fade_symmetry();
	if (symmetry_result != 0)
		return symmetry_result;
	const int timeout_result = test_timeout();
	return timeout_result != 0 ? timeout_result : test_stuck_drawing();
}
