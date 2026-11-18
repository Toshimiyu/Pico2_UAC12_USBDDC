#ifndef FEEDBACK_ANTI_WINDUP_H
#define FEEDBACK_ANTI_WINDUP_H

static inline float feedback_conditional_integral(float current, float increment,
	float proportional, float output_limit, float integral_limit)
{
	float next = current + increment;
	if (next > integral_limit)
		next = integral_limit;
	else if (next < -integral_limit)
		next = -integral_limit;

	const float output = proportional + next;
	if ((output >= output_limit && increment > 0.0f) ||
		(output <= -output_limit && increment < 0.0f))
		return current;
	return next;
}

#endif
