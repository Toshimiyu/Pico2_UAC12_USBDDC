#include "feedback_anti_windup.h"

int main(void)
{
	if (feedback_conditional_integral(0.0f, 0.5f, 1.0f, 4.0f, 3.0f) != 0.5f ||
		feedback_conditional_integral(0.0f, -0.5f, -1.0f, 4.0f, 3.0f) != -0.5f)
		return 1;
	if (feedback_conditional_integral(1.0f, 0.5f, 3.0f, 4.0f, 3.0f) != 1.0f ||
		feedback_conditional_integral(-1.0f, -0.5f, -3.0f, 4.0f, 3.0f) != -1.0f)
		return 2;
	if (feedback_conditional_integral(1.0f, -0.5f, 5.0f, 4.0f, 3.0f) != 0.5f ||
		feedback_conditional_integral(-1.0f, 0.5f, -5.0f, 4.0f, 3.0f) != -0.5f)
		return 3;
	if (feedback_conditional_integral(2.5f, 1.0f, 0.0f, 4.0f, 3.0f) != 3.0f ||
		feedback_conditional_integral(-2.5f, -1.0f, 0.0f, 4.0f, 3.0f) != -3.0f)
		return 4;
	if (feedback_conditional_integral(0.0f, 0.5f, 3.5f, 4.0f, 3.0f) != 0.0f ||
		feedback_conditional_integral(0.0f, -0.5f, -3.5f, 4.0f, 3.0f) != 0.0f)
		return 5;
	return 0;
}
