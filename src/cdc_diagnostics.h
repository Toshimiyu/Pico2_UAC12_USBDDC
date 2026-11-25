#ifndef CDC_DIAGNOSTICS_H
#define CDC_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>
#include "usb_device_control.h"
#include "dma_pcm_trace.h"
#if FB_EXPERIMENTAL_OFFSET_SEED
#include "feedback_offset_estimator.h"
#endif

typedef enum
{
	TRACE_STATE,
	TRACE_SILENCE_START,
	TRACE_SILENCE_RESUME,
	TRACE_SILENCE_STOP,
	TRACE_PCM_START,
	TRACE_PCM_END,
	TRACE_PCM_ABORT
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
void cdc_diagnostics_log_underflow(const underflow_snapshot_t *snapshot);
void cdc_diagnostics_log_state_change(usb_audio_state_t previous_state, usb_audio_state_t state, bool session_start);
void cdc_diagnostics_trace_state(usb_audio_state_t previous_state, usb_audio_state_t state);
void cdc_diagnostics_silence_block(uint32_t frames);
void cdc_diagnostics_silence_end(bool stopped);
void cdc_diagnostics_trace_pcm(dma_pcm_trace_event_t event);
bool cdc_diagnostics_get_trace(uint32_t age, cdc_trace_t *event);
void cdc_diagnostics_get_trace_summary(cdc_trace_summary_t *summary);
void cdc_diagnostics_record_lcd_time(uint32_t duration_us);
void cdc_diagnostics_record_fft_time(uint32_t duration_us);
#if USB_CDC_ENABLED
void cdc_diagnostics_record_task_gap(uint32_t now_us, usb_audio_state_t state);
#endif
void cdc_diagnostics_task(void);
#if FB_EXPERIMENTAL_OFFSET_SEED
void cdc_diagnostics_offset_seed(uint64_t uptime_us, uint32_t rate, bool valid,
	float saved_ppm, bool have_stop, uint64_t age_us);
void cdc_diagnostics_offset_update(uint64_t uptime_us, uint32_t rate,
	const feedback_offset_result_t *result);
#endif

#endif
