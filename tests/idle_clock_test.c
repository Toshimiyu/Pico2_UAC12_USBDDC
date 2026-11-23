#include "idle_clock.h"

int main(void)
{
	idle_clock_timer_t timer = {0};
	if (idle_clock_expired(&timer, 100u, 0u, false) ||
		idle_clock_expired(&timer, 1000099u, 0u, false) ||
		!idle_clock_expired(&timer, 1000100u, 0u, false))
		return 1;

	if (idle_clock_expired(&timer, 1000101u, 1u, false) ||
		idle_clock_expired(&timer, 2000100u, 1u, false) ||
		!idle_clock_expired(&timer, 2000101u, 1u, false))
		return 2;

	if (idle_clock_expired(&timer, 3000000u, 1u, true) ||
		idle_clock_expired(&timer, 3999999u, 1u, false) ||
		!idle_clock_expired(&timer, 4000000u, 1u, false))
		return 3;

	timer.initialized = false;
	if (idle_clock_expired(&timer, UINT32_MAX - 100u, 2u, false) ||
		idle_clock_expired(&timer, 999898u, 2u, false) ||
		!idle_clock_expired(&timer, 999899u, 2u, false))
		return 4;
	return 0;
}
