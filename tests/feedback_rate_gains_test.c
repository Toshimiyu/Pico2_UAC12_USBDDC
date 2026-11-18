#include "feedback_rate_gains.h"

static float magnitude(float value)
{
	return value < 0.0f ? -value : value;
}

int main(void)
{
	const uint32_t rates[] = {44100u, 48000u, 88200u, 96000u, 176400u, 192000u};
	const float expected_kp[] = {0.020727f, 0.02256f, 0.041454f,
		0.04512f, 0.082908f, 0.09024f};
	const float expected_ki[] = {0.0001074021f, 0.0001272384f, 0.0004296085f,
		0.0005089536f, 0.0017184341f, 0.0020358144f};
	for (unsigned i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i)
	{
		const float kp = feedback_rate_kp(rates[i]);
		if (magnitude(kp - expected_kp[i]) > 0.0000001f ||
			magnitude(feedback_rate_ki(kp) - expected_ki[i]) > 0.00000001f)
			return (int)i + 1;
	}
	return 0;
}
