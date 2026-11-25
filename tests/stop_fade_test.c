#include <limits.h>
#include "stop_fade.h"

static bool close_to(int32_t actual, int32_t expected, int32_t input)
{
	const int64_t difference = (int64_t)actual - expected;
	const uint64_t absolute_difference = difference < 0
		? (uint64_t)(-difference) : (uint64_t)difference;
	const int64_t absolute_input = input < 0 ? -(int64_t)input : input;
	const uint64_t tolerance = ((uint64_t)absolute_input + 65535u) / 65536u + 1u;
	return absolute_difference <= tolerance;
}

int main(void)
{
	mute_ramp_t ramp;
	stop_fade_reset(&ramp, 250u);
	int32_t first[] = {1000, -1000, 1000, -1000};
	stop_fade_apply(&ramp, first, 2u);
	if (!close_to(first[0], 800, 1000) || !close_to(first[1], -800, -1000) ||
		!close_to(first[2], 600, 1000) || !close_to(first[3], -600, -1000))
		return 1;

	int32_t second[] = {1000, -1000, INT32_MIN, INT32_MAX, 1000, -1000, 1000, -1000};
	stop_fade_apply(&ramp, second, 4u);
	if (!close_to(second[0], 400, 1000) || !close_to(second[1], -400, -1000) ||
		!close_to(second[2], (int32_t)((int64_t)INT32_MIN / 5), INT32_MIN) ||
		!close_to(second[3], (int32_t)((int64_t)INT32_MAX / 5), INT32_MAX) ||
		second[4] != 0 || second[5] != 0 || second[6] != 0 || second[7] != 0)
		return 2;

	stop_fade_reset(&ramp, 44100u);
	if (ramp.frames != 882u || ramp.position != 882u)
		return 3;
	for (uint32_t i = 0; i < 882u; i++)
	{
		int32_t samples[] = {882000, -882000};
		stop_fade_apply(&ramp, samples, 1u);
		const int32_t expected = (int32_t)(881u - i) * 1000;
		if (!close_to(samples[0], expected, 882000) ||
			!close_to(samples[1], -expected, -882000))
			return 4;
	}
	int32_t silent[] = {0, 0};
	stop_fade_apply(&ramp, silent, 1u);
	if (silent[0] != 0 || silent[1] != 0)
		return 5;
	stop_fade_reset(&ramp, 48000u);
	if (ramp.frames != 960u || ramp.position != 960u)
		return 6;
	stop_fade_reset(&ramp, UINT32_MAX);
	const uint32_t expected_max_rate_frames =
		(uint32_t)(((uint64_t)UINT32_MAX * STOP_FADE_MS + 999u) / 1000u);
	if (ramp.frames != expected_max_rate_frames)
		return 7;
	return 0;
}
