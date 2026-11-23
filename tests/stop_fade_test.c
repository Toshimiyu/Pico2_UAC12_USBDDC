#include <limits.h>
#include "stop_fade.h"

int main(void)
{
	mute_ramp_t ramp;
	stop_fade_reset(&ramp, 250u);
	int32_t first[] = {1000, -1000, 1000, -1000};
	stop_fade_apply(&ramp, first, 2u);
	if (first[0] != 800 || first[1] != -800 ||
		first[2] != 600 || first[3] != -600)
		return 1;

	int32_t second[] = {1000, -1000, INT32_MIN, INT32_MAX, 1000, -1000, 1000, -1000};
	stop_fade_apply(&ramp, second, 4u);
	if (second[0] != 400 || second[1] != -400 ||
		second[2] != (int32_t)((int64_t)INT32_MIN / 5) ||
		second[3] != (int32_t)((int64_t)INT32_MAX / 5) ||
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
		if (samples[0] != expected || samples[1] != -expected)
			return 4;
	}
	int32_t silent[] = {0, 0};
	stop_fade_apply(&ramp, silent, 1u);
	if (silent[0] != 0 || silent[1] != 0)
		return 5;
	stop_fade_reset(&ramp, 48000u);
	if (ramp.frames != 960u || ramp.position != 960u)
		return 6;
	return 0;
}
