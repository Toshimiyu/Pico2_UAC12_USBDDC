#ifndef FEEDBACK_RATE_GAINS_H
#define FEEDBACK_RATE_GAINS_H

#include <stdint.h>

#define FEEDBACK_RATE_KP_PPM_PER_FRAME (0.47f)

static inline float feedback_rate_kp(uint32_t sample_rate)
{
	return (float)sample_rate * (FEEDBACK_RATE_KP_PPM_PER_FRAME * 1e-6f);
}

static inline float feedback_rate_ki(float kp)
{
	return kp * kp * 0.25f;
}

#endif
