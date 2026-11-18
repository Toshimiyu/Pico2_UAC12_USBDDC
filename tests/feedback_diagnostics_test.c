#include <string.h>
#include "feedback_diagnostics.h"

int main(void)
{
	if (feedback_diag_limit(4.0f, 4.0f) != 1 ||
		feedback_diag_limit(-4.0f, 4.0f) != -1 ||
		feedback_diag_limit(3.9f, 4.0f) != 0 ||
		feedback_diag_limit(-3.9f, 4.0f) != 0)
		return 1;

	char output[112];
	if (feedback_diag_format(output, sizeof(output), NULL, 0) < 0 ||
		strcmp(output, ",,,,,,,") != 0)
		return 2;

	feedback_diagnostics_t diag = {
		.update_us = UINT32_MAX - 99u, .sample_rate = 48000u,
		.p_hz = 1.25f, .hf_hz = -0.5f, .adjustment_hz = -4.8f,
		.limit_hz = 4.8f, .output_saturation = -1, .integral_saturation = 1,
		.mode = FEEDBACK_DIAG_PI
	};
	if (feedback_diag_format(output, sizeof(output), &diag, 100u) < 0 ||
		strcmp(output, "PI,200,1250,-500,-1000,1000,-1,1") != 0)
		return 3;

	diag.mode = FEEDBACK_DIAG_BUFFERING;
	diag.adjustment_hz = 24.0f;
	if (feedback_diag_format(output, sizeof(output), &diag, 100u) < 0 ||
		strcmp(output, "BUFFERING,200,,,5000,,,") != 0)
		return 4;
	diag.mode = FEEDBACK_DIAG_FIXED;
	if (feedback_diag_format(output, sizeof(output), &diag, 100u) < 0 ||
		strcmp(output, "FIXED,200,,,5000,,,") != 0)
		return 5;
	diag.sample_rate = 0;
	if (feedback_diag_format(output, sizeof(output), &diag, 100u) >= 0)
		return 6;

	feedback_diag_config_t config = {
		.target_frames = 2000, .kp_microhz_per_frame = 250000,
		.ki_microhz_per_frame_s = 125000, .gain_start_permille = 500,
		.gain_ramp_us = 2000000, .boost_start_frames = 100,
		.boost_full_frames = 400, .base_limit_ppm = 50,
		.boost_limit_ppm = 700, .fixed_test = true
	};
	char header[512];
	if (feedback_diag_format_config(header, sizeof(header), &config) < 0 ||
		strcmp(header,
			"#FB_CONFIG,TARGET_FRAMES=2000,KP_UHZ_PER_FRAME=250000,KI_UHZ_PER_FRAME_S=125000,"
			"GAIN_START_PERMILLE=500,GAIN_RAMP_US=2000000,BOOST_START_ERR_FRAMES=100,"
			"BOOST_FULL_ERR_FRAMES=400,BASE_LIMIT_PPM=50,BOOST_LIMIT_PPM=700,"
			"BOOST_SOURCE=FILTERED_ERROR,FIXED_TEST=1\r\n") != 0)
		return 7;
	config.rate_scaled = true;
	config.gain_reference_hz = 44100u;
	config.kp_nppm_per_frame = 470u;
	if (feedback_diag_format_config(header, sizeof(header), &config) < 0 ||
		strstr(header, ",GAIN_MODE=RATE_SCALED,GAIN_REFERENCE_HZ=44100,"
			"KP_NPPM_PER_FRAME=470,KI_FORMULA=KP_SQUARED_DIV4\r\n") == NULL)
		return 8;
	char short_header[320];
	if (feedback_diag_format_config(short_header, sizeof(short_header), &config) <
		(int)sizeof(short_header))
		return 9;
	return 0;
}
