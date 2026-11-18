#ifndef FEEDBACK_DIAGNOSTICS_H
#define FEEDBACK_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum
{
	FEEDBACK_DIAG_PI,
	FEEDBACK_DIAG_BUFFERING,
	FEEDBACK_DIAG_FIXED
} feedback_diag_mode_t;

typedef struct
{
	uint32_t update_us;
	uint32_t sample_rate;
	float p_hz;
	float hf_hz;
	float adjustment_hz;
	float limit_hz;
	int8_t output_saturation;
	int8_t integral_saturation;
	feedback_diag_mode_t mode;
} feedback_diagnostics_t;

typedef struct
{
	uint32_t target_frames;
	uint32_t kp_microhz_per_frame;
	uint32_t ki_microhz_per_frame_s;
	uint32_t gain_start_permille;
	uint32_t gain_ramp_us;
	uint32_t boost_start_frames;
	uint32_t boost_full_frames;
	uint32_t base_limit_ppm;
	uint32_t boost_limit_ppm;
	bool fixed_test;
	uint32_t gain_reference_hz;
	uint32_t kp_nppm_per_frame;
	bool rate_scaled;
} feedback_diag_config_t;

static inline int feedback_diag_format_config(char *buffer, size_t size,
	const feedback_diag_config_t *config)
{
	const int written = snprintf(buffer, size,
		"#FB_CONFIG,TARGET_FRAMES=%lu,KP_UHZ_PER_FRAME=%lu,KI_UHZ_PER_FRAME_S=%lu,"
		"GAIN_START_PERMILLE=%lu,GAIN_RAMP_US=%lu,BOOST_START_ERR_FRAMES=%lu,"
		"BOOST_FULL_ERR_FRAMES=%lu,BASE_LIMIT_PPM=%lu,BOOST_LIMIT_PPM=%lu,"
		"BOOST_SOURCE=FILTERED_ERROR,FIXED_TEST=%u",
		(unsigned long)config->target_frames, (unsigned long)config->kp_microhz_per_frame,
		(unsigned long)config->ki_microhz_per_frame_s, (unsigned long)config->gain_start_permille,
		(unsigned long)config->gain_ramp_us, (unsigned long)config->boost_start_frames,
		(unsigned long)config->boost_full_frames, (unsigned long)config->base_limit_ppm,
		(unsigned long)config->boost_limit_ppm, (unsigned)config->fixed_test);
	if (written < 0 || (size_t)written >= size)
		return written;
	const int suffix = config->rate_scaled
		? snprintf(buffer + written, size - (size_t)written,
			",GAIN_MODE=RATE_SCALED,GAIN_REFERENCE_HZ=%lu,KP_NPPM_PER_FRAME=%lu,"
			"KI_FORMULA=KP_SQUARED_DIV4\r\n", (unsigned long)config->gain_reference_hz,
			(unsigned long)config->kp_nppm_per_frame)
		: snprintf(buffer + written, size - (size_t)written, "\r\n");
	return suffix < 0 ? suffix : written + suffix;
}

static inline int8_t feedback_diag_limit(float value, float limit)
{
	return value >= limit ? 1 : (value <= -limit ? -1 : 0);
}

static inline int feedback_diag_format(char *buffer, size_t size,
	const feedback_diagnostics_t *diag, uint32_t now_us)
{
	if (diag == NULL)
		return snprintf(buffer, size, ",,,,,,,");
	if (diag->sample_rate == 0u)
		return -1;

	const uint32_t age_us = now_us - diag->update_us;
	const int32_t adjustment_ppm_x10 =
		(int32_t)(diag->adjustment_hz * 10000000.0f / (float)diag->sample_rate);
	if (diag->mode == FEEDBACK_DIAG_PI)
		return snprintf(buffer, size, "PI,%lu,%ld,%ld,%ld,%ld,%d,%d",
			(unsigned long)age_us, (long)(int32_t)(diag->p_hz * 1000.0f),
			(long)(int32_t)(diag->hf_hz * 1000.0f), (long)adjustment_ppm_x10,
			(long)(int32_t)(diag->limit_hz * 10000000.0f / (float)diag->sample_rate),
			(int)diag->output_saturation, (int)diag->integral_saturation);
	if (diag->mode == FEEDBACK_DIAG_BUFFERING || diag->mode == FEEDBACK_DIAG_FIXED)
		return snprintf(buffer, size, "%s,%lu,,,%ld,,,",
			diag->mode == FEEDBACK_DIAG_BUFFERING ? "BUFFERING" : "FIXED",
			(unsigned long)age_us, (long)adjustment_ppm_x10);
	return -1;
}

#endif
