#ifndef USB_PACKET_TRACE_H
#define USB_PACKET_TRACE_H

#include <stdbool.h>
#include <stdint.h>

#define USB_PACKET_TRACE_CAPACITY 1024u

typedef struct
{
    uint32_t timestamp_us;
    uint16_t length;
    uint8_t endpoint;
    uint8_t alt;
} usb_packet_trace_entry_t;

typedef struct
{
    usb_packet_trace_entry_t entries[USB_PACKET_TRACE_CAPACITY];
    uint32_t start_rate;
    uint16_t count;
    bool frozen;
} usb_packet_trace_t;

static inline void usb_packet_trace_arm(usb_packet_trace_t *trace)
{
    trace->count = 0;
    trace->start_rate = 0;
    trace->frozen = false;
}

static inline void usb_packet_trace_stop(usb_packet_trace_t *trace)
{
    if (trace->count != 0)
        trace->frozen = true;
}

static inline void usb_packet_trace_add(usb_packet_trace_t *trace, uint32_t timestamp_us,
                                        uint16_t length, uint8_t endpoint, uint8_t alt,
                                        uint32_t rate)
{
    if (trace->frozen)
        return;
    if (trace->count == 0)
        trace->start_rate = rate;
    trace->entries[trace->count++] = (usb_packet_trace_entry_t){
        timestamp_us, length, endpoint, alt
    };
    if (trace->count == USB_PACKET_TRACE_CAPACITY)
        trace->frozen = true;
}

#endif
