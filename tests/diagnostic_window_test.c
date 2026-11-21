#include "diagnostic_window.h"

int main(void)
{
	diagnostic_window_t window = {0};
	diagnostic_window_sample(&window, true, 0xffff0000u, 2048);
	diagnostic_window_sample(&window, true, 0xffff1000u, 2043);
	diagnostic_window_sample(&window, true, 0xffff2000u, 2052);
	if (window.valid)
		return 1;
	diagnostic_window_sample(&window, true, 0xffff0000u + 1000000u, 2048);
	if (!window.valid || window.last_min != 2043 || window.last_max != 2052)
		return 2;
	diagnostic_window_sample(&window, true, 0xffff0000u + 2000000u, 2049);
	if (window.last_min != 2048 || window.last_max != 2049)
		return 3;
	diagnostic_window_sample(&window, false, 0, 0);
	if (window.active || window.valid)
		return 4;
	diagnostic_window_sample(&window, true, 0, 2000);
	if (!window.active || window.valid)
		return 5;
	return 0;
}
