#ifndef FEEDBACK_OFFSET_ESTIMATOR_H
#define FEEDBACK_OFFSET_ESTIMATOR_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define FEEDBACK_OFFSET_WARMUP_US 5000000u
#define FEEDBACK_OFFSET_HALF_US 15000000u
#define FEEDBACK_OFFSET_WINDOW_US (2u * FEEDBACK_OFFSET_HALF_US)
#define FEEDBACK_OFFSET_EXPIRY_US 3600000000ull

typedef enum
{
	OFFSET_ACCEPT_INITIAL,
	OFFSET_ACCEPT_SMOOTHED,
	OFFSET_REJECT_BOOST,
	OFFSET_REJECT_SATURATION,
	OFFSET_REJECT_RANGE,
	OFFSET_REJECT_PENDING,
	OFFSET_REJECT_GAP
} feedback_offset_reason_t;

typedef struct
{
	float estimate_ppm;
	float correction_ppm;
	float slope_frames_s;
	float saturation_pct;
	float saved_ppm;
	bool saved_valid;
	feedback_offset_reason_t reason;
} feedback_offset_result_t;

typedef struct
{
	float saved_ppm;
	float pending_ppm;
	bool saved_valid;
	bool pending_valid;
	bool have_stop;
	uint64_t last_stop_us;
	uint64_t last_sample_us;
	uint32_t warmup_us;
	uint32_t window_us;
	int64_t correction_sum;
	int64_t level_sum[2];
	uint32_t saturation_us;
	bool boosted;
} feedback_offset_t;

static inline void feedback_offset_clear_window(feedback_offset_t *offset, uint64_t now)
{
	offset->last_sample_us = now;
	offset->warmup_us = FEEDBACK_OFFSET_WARMUP_US;
	offset->window_us = 0;
	offset->correction_sum = 0;
	offset->level_sum[0] = 0;
	offset->level_sum[1] = 0;
	offset->saturation_us = 0;
	offset->boosted = false;
	offset->pending_valid = false;
}

static inline void feedback_offset_invalidate(feedback_offset_t *offset, uint64_t now)
{
	offset->saved_valid = false;
	offset->have_stop = false;
	feedback_offset_clear_window(offset, now);
}

static inline bool feedback_offset_start(feedback_offset_t *offset, uint64_t now)
{
	if (offset->saved_valid && offset->have_stop &&
		now - offset->last_stop_us >= FEEDBACK_OFFSET_EXPIRY_US)
		offset->saved_valid = false;
	feedback_offset_clear_window(offset, now);
	return offset->saved_valid;
}

static inline void feedback_offset_stop(feedback_offset_t *offset, uint64_t now)
{
	offset->last_stop_us = now;
	offset->have_stop = true;
	feedback_offset_clear_window(offset, now);
}

static inline feedback_offset_result_t feedback_offset_finish(feedback_offset_t *offset,
	uint32_t rate)
{
	feedback_offset_result_t result;
	const float first = (float)offset->level_sum[0] / (float)FEEDBACK_OFFSET_HALF_US / 10.0f;
	const float second = (float)offset->level_sum[1] / (float)FEEDBACK_OFFSET_HALF_US / 10.0f;
	result.slope_frames_s = (second - first) / 15.0f;
	result.correction_ppm =
		(float)offset->correction_sum / (float)FEEDBACK_OFFSET_WINDOW_US / 10.0f;
	result.saturation_pct =
		(float)offset->saturation_us * 100.0f / (float)FEEDBACK_OFFSET_WINDOW_US;
	result.estimate_ppm = result.correction_ppm - result.slope_frames_s * 1000000.0f / (float)rate;
	if (offset->boosted)
		result.reason = OFFSET_REJECT_BOOST;
	else if (offset->saturation_us > FEEDBACK_OFFSET_WINDOW_US / 2u)
		result.reason = OFFSET_REJECT_SATURATION;
	else if (!isfinite(result.estimate_ppm) || fabsf(result.estimate_ppm) > 80.0f)
	{
		offset->saved_valid = false;
		result.reason = OFFSET_REJECT_RANGE;
	}
	else if (!offset->saved_valid)
	{
		offset->saved_ppm = result.estimate_ppm;
		offset->saved_valid = true;
		result.reason = OFFSET_ACCEPT_INITIAL;
	}
	else if (fabsf(result.estimate_ppm - offset->saved_ppm) >= 15.0f &&
		(!offset->pending_valid || fabsf(result.estimate_ppm - offset->pending_ppm) > 5.0f))
	{
		offset->pending_ppm = result.estimate_ppm;
		offset->pending_valid = true;
		result.reason = OFFSET_REJECT_PENDING;
	}
	else
	{
		offset->saved_ppm = 0.7f * offset->saved_ppm + 0.3f * result.estimate_ppm;
		result.reason = OFFSET_ACCEPT_SMOOTHED;
	}
	if (result.reason != OFFSET_REJECT_PENDING)
		offset->pending_valid = false;
	result.saved_valid = offset->saved_valid;
	result.saved_ppm = offset->saved_ppm;
	return result;
}

static inline bool feedback_offset_sample(feedback_offset_t *offset, uint64_t now,
	uint32_t rate, float level, float correction_hz, bool saturated, bool boosted,
	feedback_offset_result_t *result)
{
	const uint64_t elapsed = now - offset->last_sample_us;
	offset->last_sample_us = now;
	if (elapsed > 20000u || rate == 0u)
	{
		*result = (feedback_offset_result_t){
			.saved_ppm = offset->saved_ppm,
			.saved_valid = offset->saved_valid,
			.reason = OFFSET_REJECT_GAP,
		};
		feedback_offset_clear_window(offset, now);
		return true;
	}
	uint32_t remaining = (uint32_t)elapsed;
	if (offset->warmup_us != 0u)
	{
		const uint32_t skipped = remaining < offset->warmup_us ? remaining : offset->warmup_us;
		offset->warmup_us -= skipped;
		remaining -= skipped;
	}
	const int32_t level_tenths = (int32_t)(level * 10.0f);
	const int32_t correction_tenths = (int32_t)(correction_hz * (10000000.0f / (float)rate));
	bool completed = false;
	while (remaining != 0u)
	{
		const unsigned half = offset->window_us >= FEEDBACK_OFFSET_HALF_US ? 1u : 0u;
		const uint32_t boundary = (half + 1u) * FEEDBACK_OFFSET_HALF_US;
		const uint32_t available = boundary - offset->window_us;
		const uint32_t weight = remaining < available ? remaining : available;
		offset->level_sum[half] += (int64_t)level_tenths * weight;
		offset->correction_sum += (int64_t)correction_tenths * weight;
		if (saturated)
			offset->saturation_us += weight;
		if (boosted)
			offset->boosted = true;
		offset->window_us += weight;
		remaining -= weight;
		if (offset->window_us == FEEDBACK_OFFSET_WINDOW_US)
		{
			*result = feedback_offset_finish(offset, rate);
			offset->window_us = 0;
			offset->correction_sum = 0;
			offset->level_sum[0] = 0;
			offset->level_sum[1] = 0;
			offset->saturation_us = 0;
			offset->boosted = false;
			completed = true;
		}
	}
	return completed;
}

#endif
