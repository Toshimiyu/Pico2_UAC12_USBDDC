#include "feedback_offset_estimator.h"

static bool near(float actual, float expected, float tolerance)
{
	return fabsf(actual - expected) <= tolerance;
}

static bool run_window(feedback_offset_t *offset, uint64_t *now, float correction_ppm,
	bool saturated, bool boosted, feedback_offset_result_t *result)
{
	bool completed = false;
	for (unsigned i = 0; i < 30000u; ++i)
	{
		*now += 1000u;
		completed = feedback_offset_sample(offset, *now, 44100u, 2048.0f,
			correction_ppm * 44100.0f * 1e-6f, saturated, boosted, result);
	}
	return completed;
}

int main(void)
{
	feedback_offset_t offset = {0};
	feedback_offset_result_t result;
	uint64_t now = 0;
	if (feedback_offset_start(&offset, now))
		return 1;
	for (unsigned i = 1; i <= 35000u; ++i)
	{
		now += 1000u;
		const float level = 2048.0f + 1.323f * ((float)i / 1000.0f);
		const bool completed = feedback_offset_sample(&offset, now, 44100u, level,
			56.0f * 44100.0f * 1e-6f, false, false, &result);
		if (completed != (i == 35000u))
			return 2;
	}
	if (result.reason != OFFSET_ACCEPT_INITIAL || !result.saved_valid ||
		!near(result.estimate_ppm, 26.0f, 0.3f))
		return 3;
	feedback_offset_t high_rate = {0};
	feedback_offset_start(&high_rate, 0);
	for (unsigned i = 1; i <= 35000u; ++i)
	{
		const float level = 2048.0f - 5.76f * ((float)i / 1000.0f);
		const bool completed = feedback_offset_sample(&high_rate, (uint64_t)i * 1000u,
			192000u, level, -4.0f * 192000.0f * 1e-6f, false, false, &result);
		if (completed != (i == 35000u))
			return 15;
	}
	if (result.reason != OFFSET_ACCEPT_INITIAL || !near(result.estimate_ppm, 26.0f, 0.3f))
		return 16;
	feedback_offset_stop(&offset, now);
	if (!feedback_offset_start(&offset, now + 2000000u))
		return 4;
	feedback_offset_stop(&offset, now + 2000000u + 19000000u);
	if (!feedback_offset_start(&offset, now + 21000000u + FEEDBACK_OFFSET_EXPIRY_US - 1u))
		return 5;
	feedback_offset_stop(&offset, now);
	if (feedback_offset_start(&offset, now + FEEDBACK_OFFSET_EXPIRY_US))
		return 6;

	offset = (feedback_offset_t){0};
	feedback_offset_start(&offset, 0);
	now = 5000000u;
	offset.warmup_us = 0;
	offset.last_sample_us = now;
	if (!run_window(&offset, &now, 26.0f, false, false, &result) ||
		result.reason != OFFSET_ACCEPT_INITIAL)
		return 7;
	if (!run_window(&offset, &now, 46.0f, false, false, &result) ||
		result.reason != OFFSET_REJECT_PENDING || !near(offset.saved_ppm, 26.0f, 0.2f))
		return 8;
	if (!run_window(&offset, &now, 47.0f, false, false, &result) ||
		result.reason != OFFSET_ACCEPT_SMOOTHED || !near(offset.saved_ppm, 32.3f, 0.2f))
		return 9;
	if (!run_window(&offset, &now, 26.0f, false, true, &result) ||
		result.reason != OFFSET_REJECT_BOOST || !offset.saved_valid)
		return 10;
	if (!run_window(&offset, &now, 26.0f, true, false, &result) ||
		result.reason != OFFSET_REJECT_SATURATION || !offset.saved_valid)
		return 11;
	for (unsigned i = 0; i < 30000u; ++i)
	{
		now += 1000u;
		const bool completed = feedback_offset_sample(&offset, now, 44100u, 2048.0f,
			26.0f * 44100.0f * 1e-6f, i < 15000u, false, &result);
		if (completed != (i == 29999u))
			return 17;
	}
	if (result.reason != OFFSET_ACCEPT_SMOOTHED || !near(result.saturation_pct, 50.0f, 0.001f))
		return 18;
	if (!run_window(&offset, &now, 90.0f, false, false, &result) ||
		result.reason != OFFSET_REJECT_RANGE || offset.saved_valid)
		return 12;

	offset.saved_valid = true;
	offset.saved_ppm = 26.0f;
	feedback_offset_start(&offset, now);
	now += 21000u;
	if (!feedback_offset_sample(&offset, now, 44100u, 2048.0f, 0.0f, false, false, &result) ||
		result.reason != OFFSET_REJECT_GAP ||
		offset.warmup_us != FEEDBACK_OFFSET_WARMUP_US || !offset.saved_valid)
		return 13;
	feedback_offset_invalidate(&offset, now);
	if (offset.saved_valid || offset.have_stop)
		return 14;
	return 0;
}
