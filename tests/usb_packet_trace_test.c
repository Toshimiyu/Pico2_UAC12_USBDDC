#include <assert.h>
#include "usb_packet_trace.h"

int main(void)
{
    usb_packet_trace_t trace = {0};
    usb_packet_trace_add(&trace, UINT32_MAX - 100u, 176, 1, 1, 44100);
    usb_packet_trace_add(&trace, 899u, 180, 1, 1, 44100);
    assert(trace.count == 2 && !trace.frozen && trace.start_rate == 44100);
    assert(trace.entries[1].timestamp_us - trace.entries[0].timestamp_us == 1000u);
    assert(trace.entries[1].length == 180 && trace.entries[1].endpoint == 1 &&
           trace.entries[1].alt == 1);
    usb_packet_trace_stop(&trace);
    usb_packet_trace_add(&trace, 1900, 0, 1, 2, 48000);
    assert(trace.frozen && trace.count == 2 && trace.start_rate == 44100);
    usb_packet_trace_arm(&trace);
    usb_packet_trace_stop(&trace);
    assert(!trace.frozen && trace.count == 0);
    for (uint32_t i = 0; i < USB_PACKET_TRACE_CAPACITY; i++)
        usb_packet_trace_add(&trace, i, (uint16_t)i, 1, 2, 48000);
    assert(trace.frozen && trace.count == USB_PACKET_TRACE_CAPACITY);
    usb_packet_trace_add(&trace, 9999, 0, 1, 1, 44100);
    assert(trace.count == USB_PACKET_TRACE_CAPACITY &&
           trace.entries[USB_PACKET_TRACE_CAPACITY - 1].length == USB_PACKET_TRACE_CAPACITY - 1);
    return 0;
}
