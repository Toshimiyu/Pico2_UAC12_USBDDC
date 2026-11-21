#include "display_rta.h"

int main(void)
{
#if USB_CDC_ENABLED
	if (display_rta_next_page(DISPLAY_PAGE_BOOT) != DISPLAY_PAGE_DIAGNOSTICS ||
		display_rta_next_page(DISPLAY_PAGE_DIAGNOSTICS) != DISPLAY_PAGE_FEEDBACK ||
		display_rta_next_page(DISPLAY_PAGE_FEEDBACK) != DISPLAY_PAGE_STARTUP_LOG ||
#if USB_PACKET_TRACE_ENABLE
		display_rta_next_page(DISPLAY_PAGE_STARTUP_LOG) != DISPLAY_PAGE_PACKET_TRACE ||
		display_rta_next_page(DISPLAY_PAGE_PACKET_TRACE) != DISPLAY_PAGE_BOOT)
#else
		display_rta_next_page(DISPLAY_PAGE_STARTUP_LOG) != DISPLAY_PAGE_BOOT)
#endif
		return 1;
#else
	if (display_rta_next_page(DISPLAY_PAGE_BOOT) != DISPLAY_PAGE_PLAYBACK ||
		display_rta_next_page(DISPLAY_PAGE_PLAYBACK) != DISPLAY_PAGE_DIAGNOSTICS ||
		display_rta_next_page(DISPLAY_PAGE_DIAGNOSTICS) != DISPLAY_PAGE_FEEDBACK ||
		display_rta_next_page(DISPLAY_PAGE_FEEDBACK) != DISPLAY_PAGE_BOOT)
		return 1;
#endif
	return 0;
}
