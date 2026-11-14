#ifndef CDC_DIAGNOSTICS_H
#define CDC_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdbool.h>
#include "usb_device_control.h"

typedef enum
{
	TRACE_STATE,
	TRACE_SILENCE_START,
	TRACE_SILENCE_RESUME,
	TRACE_SILENCE_STOP
} cdc_trace_kind_t;

typedef struct
{
	uint64_t uptime_us;
	uint32_t sequence;
	uint32_t rate;
	uint32_t ring;
	uint32_t dma_pool;
	uint32_t silence_frames;
	usb_audio_state_t from;
	usb_audio_state_t to;
	cdc_trace_kind_t kind;
} cdc_trace_t;

typedef struct
{
	uint32_t silence_episodes;
	uint32_t silence_frames;
	uint32_t history_overwritten;
} cdc_trace_summary_t;

void cdc_diagnostics_init(void);
bool cdc_diagnostics_is_connected(void);
bool cdc_diagnostics_is_active(void);
void cdc_diagnostics_log_anomaly(uint8_t code, const char *detail_name, uint32_t detail_value);
void cdc_diagnostics_log_state_change(usb_audio_state_t previous_state, usb_audio_state_t state, bool session_start);
void cdc_diagnostics_trace_state(usb_audio_state_t previous_state, usb_audio_state_t state);
void cdc_diagnostics_silence_block(uint32_t frames);
void cdc_diagnostics_silence_end(bool stopped);
bool cdc_diagnostics_get_trace(uint32_t age, cdc_trace_t *event);
void cdc_diagnostics_get_trace_summary(cdc_trace_summary_t *summary);
void cdc_diagnostics_record_lcd_time(uint32_t duration_us);
void cdc_diagnostics_record_fft_time(uint32_t duration_us);
void cdc_diagnostics_task(void);

#endif
