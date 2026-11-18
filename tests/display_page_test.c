#include "display_rta.h"

int main(void)
{
#if USB_CDC_ENABLED
	if (display_rta_next_page(DISPLAY_PAGE_BOOT) != DISPLAY_PAGE_DIAGNOSTICS ||
		display_rta_next_page(DISPLAY_PAGE_DIAGNOSTICS) != DISPLAY_PAGE_STARTUP_LOG ||
		display_rta_next_page(DISPLAY_PAGE_STARTUP_LOG) != DISPLAY_PAGE_BOOT)
		return 1;
#else
	if (display_rta_next_page(DISPLAY_PAGE_BOOT) != DISPLAY_PAGE_PLAYBACK ||
		display_rta_next_page(DISPLAY_PAGE_PLAYBACK) != DISPLAY_PAGE_DIAGNOSTICS ||
		display_rta_next_page(DISPLAY_PAGE_DIAGNOSTICS) != DISPLAY_PAGE_BOOT)
		return 1;
#endif
	return 0;
}
