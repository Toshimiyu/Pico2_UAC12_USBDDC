#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "cdc_diagnostics.h"
#include "common.h"
#include "debug_gauge.h"
#include "display_rta.h"
#include "spectrum.h"
#include "transmit_to_dac.h"
#include "usb_audio_config.h"
#include "usb_device_control.h"

#if USB_CDC_ENABLED

#define CDC_LOG_PERIOD_US 100000u
#define CDC_RING_AVERAGE_PERIOD_US 1000u
#define CDC_EVENT_QUEUE_SIZE 16u
#define CDC_CSV_LINE_SIZE 480u
#define CDC_MEMORY_LINE_SIZE 128u
#define STRINGIFY_DETAIL(value) #value
#define STRINGIFY(value) STRINGIFY_DETAIL(value)
#define CDC_CSV_HEADER \
	"MS,FREQ,BIT_DEPTH,USB_MODE,HOST_RATE,FEEDBACK_RATE,HF_DIFF_NOW,RING_NOW,RING_MIN,RING_AVG,RING_MAX,MAX_HF_DIFF_SESSION,MAX_HF_DIFF_STREAM,STATE,UNDERFLOW_COUNT,USB_GAP_MAX_US,USBBUF_CAPACITY,USBBUF_FREE_NOW,USBBUF_FREE_MIN_STREAM,USBBUF_FREE_MAX_STREAM,USBBUF_FREE_MIN_PCT_STREAM,LCD_MAX_US,LCD_AVG_US,LCD_COUNT,FFT_MAX_US,FFT_AVG_US,FFT_COUNT,RATE_CHANGE_COUNT,STREAM_START_COUNT,STREAM_STOP_COUNT,LOOP0_LT1MS,LOOP0_LT2MS,LOOP0_LT5MS,LOOP0_GE5MS,LOOP1_LT1MS,LOOP1_LT2MS,LOOP1_LT5MS,LOOP1_GE5MS\r\n"

_Static_assert(CDC_CSV_LINE_SIZE <= CFG_TUD_CDC_TX_BUFSIZE,
	"CDC log line buffer must fit in the TinyUSB CDC TX FIFO");
_Static_assert(sizeof(CDC_CSV_HEADER) <= CFG_TUD_CDC_TX_BUFSIZE,
	"CDC CSV header must fit in the TinyUSB CDC TX FIFO");

#if USB_AUDIO_CLASS_VERSION == 1
#define USB_MODE_NAME "UAC1"
#else
#define USB_MODE_NAME "UAC2"
#endif

#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

extern uint8_t __data_start__[];
extern uint8_t __bss_end__[];

typedef struct
{
	uint32_t timestamp_us;
	uint32_t detail_value;
	uint32_t sample_rate;
	uint32_t ring_frames;
	uint32_t dma_pool_frames;
	uint8_t code;
	usb_audio_state_t state;
	char detail_name[20];
} cdc_anomaly_t;

typedef struct
{
	uint32_t total_us;
	uint32_t max_us;
	uint32_t count;
} cdc_timing_window_t;

static spin_lock_t *event_lock;
static cdc_anomaly_t event_queue[CDC_EVENT_QUEUE_SIZE];
static uint8_t event_head;
static uint8_t event_count;
static uint32_t event_drop_count;
static uint32_t row_drop_count;
static uint8_t session_header_pos;
static volatile bool session_active;
static uint32_t session_start_us;
static uint32_t next_sample_us;
static uint32_t next_ring_average_us;
static uint64_t ring_average_sum;
static uint32_t ring_average_count;
static int32_t ring_min;
static int32_t ring_max;
static uint32_t max_hf_diff_session;
static uint32_t session_underflow_start;
static uint32_t session_switch_start;
static uint32_t stream_start_count;
static uint32_t stream_stop_count;
static cdc_timing_window_t lcd_timing;
static cdc_timing_window_t fft_timing;

static const char *anomaly_name(uint8_t code)
{
	switch (code)
	{
	case 1: return "UNDERFLOW";
	case 2: return "OVERFLOW";
	case 3: return "FIFO_RESYNC";
	case 4: return "USB_EVENT";
	case 5: return "OUTPUT_STOP";
	case 6: return "CORE0_LOOP_GAP";
	case 7: return "CORE1_LOOP_GAP";
	case 8: return "USB_OUT_GAP";
	case 9: return "PIO_TXSTALL";
	case 10: return "DMA_AHB_ERROR";
	case 11: return "PCM_FORMAT";
	case 12: return "PCM_FRAME";
	case 13: return "PCM_CLICK_CANDIDATE";
	default: return "UNKNOWN";
	}
}

static const char *audio_state_name(usb_audio_state_t state)
{
	switch (state)
	{
	case USB_AUDIO_IDLE: return "IDLE";
	case USB_AUDIO_BUFFERING: return "BUFFERING";
	case USB_AUDIO_STREAMING: return "STREAMING";
	case USB_AUDIO_OVERFLOW_FATAL: return "OVERFLOW_FATAL";
	case USB_AUDIO_UNDERFLOW_FATAL: return "UNDERFLOW_FATAL";
	case USB_AUDIO_SWITCHING: return "SWITCHING";
	default: return "UNKNOWN";
	}
}

static bool cdc_write(const char *text, uint32_t length)
{
	if (tud_cdc_write_available() < length)
		return false;

	if (tud_cdc_write(text, length) != length)
		return false;

	tud_cdc_write_flush();
	return true;
}

static uint32_t session_milliseconds(uint32_t timestamp_us)
{
	return (timestamp_us - session_start_us) / 1000u;
}

static void reset_event_queue(void)
{
	const uint32_t owner = spin_lock_blocking(event_lock);
	event_head = 0;
	event_count = 0;
	event_drop_count = 0;
	spin_unlock(event_lock, owner);
}

static bool start_session(uint32_t now)
{
	const char *headers[] = {
		"#FW_VERSION=" FW_VERSION ",PREFILL_FRAMES=" STRINGIFY(USB_AUDIO_PREFILL_FRAMES) ",PREFILL_MS=" STRINGIFY(TARGET_PREFILL_MS) ",USB_MODE=" USB_MODE_NAME "\r\n",
		NULL,
		CDC_CSV_HEADER,
		"STATE,MS,FROM,TO,RATE,RING,DMA_POOL,USB_MODE\r\n",
		"ANOMALY,MS,CODE,DETAIL,USB_MODE,STATE,RATE,RING,DMA_POOL\r\n"
	};
	char memory_header[CDC_MEMORY_LINE_SIZE];
	const uint32_t static_ram_bytes =
		(uint32_t)((uintptr_t)__bss_end__ - (uintptr_t)__data_start__);
	const uint32_t ring_alloc_bytes =
		(uint32_t)(SIZE_AUDIO_RING_FRAMES * 2u * sizeof(int32_t));
	const int memory_header_length = snprintf(memory_header, sizeof(memory_header),
		"#MEM,SRAM_DATA_BSS_BYTES=%lu,RING_ALLOC_BYTES=%lu,FFT_STATIC_BYTES=%lu,LCD_BUFFER_BYTES=%lu\r\n",
		(unsigned long)static_ram_bytes,
		(unsigned long)ring_alloc_bytes,
		(unsigned long)spectrum_static_ram_bytes(),
		(unsigned long)(display_rta_buffer_bytes() + debug_gauge_buffer_bytes()));
	if (memory_header_length < 0 || (size_t)memory_header_length >= sizeof(memory_header))
		return false;
	headers[1] = memory_header;
	while (session_header_pos < sizeof(headers) / sizeof(headers[0]))
	{
		const char *header = headers[session_header_pos];
		if (!cdc_write(header, (uint32_t)strlen(header)))
			return false;
		session_header_pos++;
	}

	reset_event_queue();
	row_drop_count = 0;
	session_start_us = now;
	next_sample_us = now;
	next_ring_average_us = now;
	ring_average_sum = 0;
	ring_average_count = 0;
	ring_min = INT32_MAX;
	ring_max = 0;
	max_hf_diff_session = 0;
	const usb_audio_stats_t *stats = usb_audio_get_stats();
	session_underflow_start = stats->underflow_count;
	session_switch_start = stats->switch_count;
	stream_start_count = usb_audio_get_state() == USB_AUDIO_STREAMING ? 1u : 0u;
	stream_stop_count = 0;
	memset(&lcd_timing, 0, sizeof(lcd_timing));
	const uint32_t fft_owner = spin_lock_blocking(event_lock);
	memset(&fft_timing, 0, sizeof(fft_timing));
	spin_unlock(event_lock, fft_owner);
	session_active = true;
	cdc_diagnostics_log_state_change(USB_AUDIO_IDLE, usb_audio_get_state(), true);
	return true;
}

static void sample_ring_level(uint32_t now)
{
	const int32_t level = (int32_t)audio_ring_total_frames();
	if (level < ring_min)
		ring_min = level;
	if (level > ring_max)
		ring_max = level;
	if ((int32_t)(now - next_ring_average_us) >= 0)
	{
		ring_average_sum += (uint32_t)(level > 0 ? level : 0);
		ring_average_count++;
		next_ring_average_us = now + CDC_RING_AVERAGE_PERIOD_US;
	}

	const uint32_t host_rate = usb_audio_get_host_rate_hz();
	const uint32_t feedback_rate = usb_audio_get_feedback_hz();
	if (host_rate != 0u && feedback_rate != 0u)
	{
		const uint32_t difference = host_rate > feedback_rate
			? host_rate - feedback_rate
			: feedback_rate - host_rate;
		if (difference > max_hf_diff_session)
			max_hf_diff_session = difference;
	}
}

static bool write_sample(uint32_t now)
{
	char line[CDC_CSV_LINE_SIZE];
	const uint32_t host_rate = usb_audio_get_host_rate_hz();
	const uint32_t feedback_rate = usb_audio_get_feedback_hz();
	const int32_t hf_diff_now = (int32_t)host_rate - (int32_t)feedback_rate;
	char hf_diff_text[16];
	if (host_rate == 0u || feedback_rate == 0u)
		hf_diff_text[0] = '\0';
	else
		snprintf(hf_diff_text, sizeof(hf_diff_text), "%ld", (long)hf_diff_now);
	const uint32_t milliseconds = session_milliseconds(now);
	const int32_t level = (int32_t)audio_ring_total_frames();
	const usb_audio_stats_t *stats = usb_audio_get_stats();
	const uint32_t ring_used_now = level > 0 ? (uint32_t)level : 0u;
	const uint32_t ring_used_min = ring_min == INT32_MAX ? ring_used_now :
		(ring_min > 0 ? (uint32_t)ring_min : 0u);
	const uint32_t ring_used_avg = ring_average_count != 0
		? (uint32_t)(ring_average_sum / ring_average_count) : ring_used_now;
	const uint32_t ring_used_max = ring_max > 0 ? (uint32_t)ring_max : 0u;
	const uint32_t ring_capacity = SIZE_AUDIO_RING_FRAMES;
	const uint32_t stream_min_used =
		(stats->level_min >= 0 && stats->level_min < (int32_t)ring_capacity)
			? (uint32_t)stats->level_min : ring_used_min;
	const uint32_t stream_max_used =
		(stats->level_max > 0 && stats->level_max <= (int32_t)ring_capacity)
			? (uint32_t)stats->level_max : ring_used_max;
	const uint32_t free_now = ring_used_now < ring_capacity
		? ring_capacity - ring_used_now : 0u;
	const uint32_t free_min = stream_max_used < ring_capacity
		? ring_capacity - stream_max_used : 0u;
	const uint32_t free_max = stream_min_used < ring_capacity
		? ring_capacity - stream_min_used : 0u;
	const uint32_t free_min_pct = (free_min * 100u) / ring_capacity;
	if (stats->underflow_count < session_underflow_start)
		session_underflow_start = 0;
	if (stats->switch_count < session_switch_start)
		session_switch_start = 0;
	const uint32_t underflow_count = stats->underflow_count >= session_underflow_start
		? stats->underflow_count - session_underflow_start : stats->underflow_count;
	const uint32_t rate_change_count = stats->switch_count >= session_switch_start
		? stats->switch_count - session_switch_start : stats->switch_count;
	const uint32_t lcd_avg_us = lcd_timing.count != 0
		? lcd_timing.total_us / lcd_timing.count : 0u;
	const cdc_timing_window_t saved_lcd_timing = lcd_timing;
	memset(&lcd_timing, 0, sizeof(lcd_timing));
	const uint32_t fft_owner = spin_lock_blocking(event_lock);
	const cdc_timing_window_t saved_fft_timing = fft_timing;
	memset(&fft_timing, 0, sizeof(fft_timing));
	spin_unlock(event_lock, fft_owner);
	const uint32_t fft_avg_us = saved_fft_timing.count != 0
		? saved_fft_timing.total_us / saved_fft_timing.count : 0u;

	int written = snprintf(line, sizeof(line),
		"%lu,%lu,%lu,%s,%lu,%lu,%s,%ld,%ld,%lu,%ld,%lu,%ld,%s",
		(unsigned long)milliseconds,
		(unsigned long)audio_state.freq,
		(unsigned long)audio_state.bit_depth,
		USB_MODE_NAME,
		(unsigned long)host_rate,
		(unsigned long)feedback_rate,
		hf_diff_text,
		(long)level,
		(long)(ring_min == INT32_MAX ? level : ring_min),
		(unsigned long)ring_used_avg,
		(long)ring_max,
		(unsigned long)max_hf_diff_session,
		(long)stats->max_hf_diff,
		audio_state_name(stats->state));

	if (written < 0 || (size_t)written >= sizeof(line))
		goto failed;

	const int appended = snprintf(line + written, sizeof(line) - (size_t)written,
		",%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
		(unsigned long)underflow_count,
		(unsigned long)stats->usb_out_gap_max_us,
		(unsigned long)ring_capacity,
		(unsigned long)free_now,
		(unsigned long)free_min,
		(unsigned long)free_max,
		(unsigned long)free_min_pct,
		(unsigned long)saved_lcd_timing.max_us,
		(unsigned long)lcd_avg_us,
		(unsigned long)saved_lcd_timing.count,
		(unsigned long)saved_fft_timing.max_us,
		(unsigned long)fft_avg_us,
		(unsigned long)saved_fft_timing.count,
		(unsigned long)rate_change_count,
		(unsigned long)stream_start_count,
		(unsigned long)stream_stop_count,
		(unsigned long)stats->loop0_hist[0],
		(unsigned long)stats->loop0_hist[1],
		(unsigned long)stats->loop0_hist[2],
		(unsigned long)stats->loop0_hist[3],
		(unsigned long)dma_diag.loop_hist[0],
		(unsigned long)dma_diag.loop_hist[1],
		(unsigned long)dma_diag.loop_hist[2],
		(unsigned long)dma_diag.loop_hist[3]);
	if (appended < 0 || (size_t)appended >= sizeof(line) - (size_t)written)
		goto failed;
	written += appended;

	if (!cdc_write(line, (uint32_t)written))
		goto failed;

	ring_min = (int32_t)ring_used_now;
	ring_max = (int32_t)ring_used_now;
	ring_average_sum = 0;
	ring_average_count = 0;
	next_sample_us = now + CDC_LOG_PERIOD_US;
	return true;

failed:
	lcd_timing.total_us += saved_lcd_timing.total_us;
	lcd_timing.count += saved_lcd_timing.count;
	if (lcd_timing.max_us < saved_lcd_timing.max_us)
		lcd_timing.max_us = saved_lcd_timing.max_us;
	const uint32_t restore_owner = spin_lock_blocking(event_lock);
	fft_timing.total_us += saved_fft_timing.total_us;
	fft_timing.count += saved_fft_timing.count;
	if (fft_timing.max_us < saved_fft_timing.max_us)
		fft_timing.max_us = saved_fft_timing.max_us;
	spin_unlock(event_lock, restore_owner);
	next_sample_us = now + CDC_LOG_PERIOD_US;
	return false;
}

static bool peek_event(cdc_anomaly_t *event)
{
	const uint32_t owner = spin_lock_blocking(event_lock);
	if (event_count == 0)
	{
		spin_unlock(event_lock, owner);
		return false;
	}

	*event = event_queue[event_head];
	spin_unlock(event_lock, owner);
	return true;
}

static void pop_event(void)
{
	const uint32_t owner = spin_lock_blocking(event_lock);
	if (event_count != 0)
	{
		event_head = (uint8_t)((event_head + 1u) % CDC_EVENT_QUEUE_SIZE);
		event_count--;
	}
	spin_unlock(event_lock, owner);
}

static void write_pending_anomalies(uint32_t now)
{
	cdc_anomaly_t event;
	char line[CDC_CSV_LINE_SIZE];

	while (peek_event(&event))
	{
		const int written = event.code == 0
			? snprintf(line, sizeof(line),
				"STATE,%lu,%s,%s,%lu,%lu,%lu,%s\r\n",
				(unsigned long)session_milliseconds(event.timestamp_us),
				event.detail_name,
				audio_state_name(event.state),
				(unsigned long)event.sample_rate,
				(unsigned long)event.ring_frames,
				(unsigned long)event.dma_pool_frames,
				USB_MODE_NAME)
			: snprintf(line, sizeof(line),
				"ANOMALY,%lu,%s,%s=%lu,%s,%s,%lu,%lu,%lu\r\n",
				(unsigned long)session_milliseconds(event.timestamp_us),
				anomaly_name(event.code),
				event.detail_name[0] ? event.detail_name : "VALUE",
				(unsigned long)event.detail_value,
				USB_MODE_NAME,
				audio_state_name(event.state),
				(unsigned long)event.sample_rate,
				(unsigned long)event.ring_frames,
				(unsigned long)event.dma_pool_frames);
		if (written < 0 || (size_t)written >= sizeof(line) ||
			!cdc_write(line, (uint32_t)written))
			return;
		pop_event();
	}

	const uint32_t owner = spin_lock_blocking(event_lock);
	const uint32_t dropped = event_drop_count;
	spin_unlock(event_lock, owner);
	if (dropped != 0)
	{
		const int written = snprintf(line, sizeof(line),
			"ANOMALY,%lu,EVENT_QUEUE_DROPPED,COUNT=%lu,%s,%s,%lu,%lu,%lu\r\n",
			(unsigned long)session_milliseconds(now), (unsigned long)dropped, USB_MODE_NAME,
			audio_state_name(usb_audio_get_state()), (unsigned long)audio_state.freq,
			(unsigned long)audio_ring_total_frames(), (unsigned long)dma_tx_pool_frames());
		if (written > 0 && (size_t)written < sizeof(line) &&
			cdc_write(line, (uint32_t)written))
		{
			const uint32_t clear_owner = spin_lock_blocking(event_lock);
			event_drop_count = event_drop_count >= dropped
				? event_drop_count - dropped : 0;
			spin_unlock(event_lock, clear_owner);
		}
	}

	if (row_drop_count != 0)
	{
		const int written = snprintf(line, sizeof(line),
			"ANOMALY,%lu,LOG_ROWS_DROPPED,COUNT=%lu,%s,%s,%lu,%lu,%lu\r\n",
			(unsigned long)session_milliseconds(now), (unsigned long)row_drop_count, USB_MODE_NAME,
			audio_state_name(usb_audio_get_state()), (unsigned long)audio_state.freq,
			(unsigned long)audio_ring_total_frames(), (unsigned long)dma_tx_pool_frames());
		if (written > 0 && (size_t)written < sizeof(line) &&
			cdc_write(line, (uint32_t)written))
			row_drop_count = 0;
	}
}

void cdc_diagnostics_init(void)
{
	if (!COM_PORT_OUTPUT_ENABLE)
		return;

	event_lock = spin_lock_init(spin_lock_claim_unused(true));
}

bool cdc_diagnostics_is_connected(void)
{
	return COM_PORT_OUTPUT_ENABLE && tud_cdc_connected();
}

bool cdc_diagnostics_is_active(void)
{
	return COM_PORT_OUTPUT_ENABLE && session_active;
}

void cdc_diagnostics_log_anomaly(uint8_t code, const char *detail_name, uint32_t detail_value)
{
	if (!COM_PORT_OUTPUT_ENABLE || !session_active || event_lock == NULL)
		return;

	const uint32_t owner = spin_lock_blocking(event_lock);
	if (event_count == CDC_EVENT_QUEUE_SIZE)
	{
		event_drop_count++;
		spin_unlock(event_lock, owner);
		return;
	}

	const uint8_t index = (uint8_t)((event_head + event_count) % CDC_EVENT_QUEUE_SIZE);
	cdc_anomaly_t *event = &event_queue[index];
	event->timestamp_us = time_us_32();
	event->code = code;
	event->detail_value = detail_value;
	event->sample_rate = audio_state.freq;
	event->ring_frames = (uint32_t)audio_ring_total_frames();
	event->dma_pool_frames = dma_tx_pool_frames();
	event->state = usb_audio_get_state();
	if (detail_name == NULL)
		event->detail_name[0] = '\0';
	else
		snprintf(event->detail_name, sizeof(event->detail_name), "%s", detail_name);
	event_count++;
	spin_unlock(event_lock, owner);
}

void cdc_diagnostics_log_state_change(usb_audio_state_t previous_state,
									  usb_audio_state_t state, bool session_start)
{
	if (!COM_PORT_OUTPUT_ENABLE || !session_active || event_lock == NULL)
		return;

	const uint32_t timestamp_us = time_us_32();
	const uint32_t sample_rate = audio_state.freq;
	const uint32_t ring_frames = (uint32_t)audio_ring_total_frames();
	const uint32_t dma_pool_frames = dma_tx_pool_frames();
	const uint32_t owner = spin_lock_blocking(event_lock);
	if (!session_start)
	{
		if (previous_state != USB_AUDIO_STREAMING && state == USB_AUDIO_STREAMING)
			stream_start_count++;
		if (previous_state == USB_AUDIO_STREAMING && state != USB_AUDIO_STREAMING)
			stream_stop_count++;
	}
	if (event_count == CDC_EVENT_QUEUE_SIZE)
	{
		event_drop_count++;
		spin_unlock(event_lock, owner);
		return;
	}

	const uint8_t index = (uint8_t)((event_head + event_count) % CDC_EVENT_QUEUE_SIZE);
	cdc_anomaly_t *event = &event_queue[index];
	event->timestamp_us = timestamp_us;
	event->detail_value = 0;
	event->sample_rate = sample_rate;
	event->ring_frames = ring_frames;
	event->dma_pool_frames = dma_pool_frames;
	event->code = 0;
	event->state = state;
	if (session_start)
		snprintf(event->detail_name, sizeof(event->detail_name), "%s", "START");
	else
		snprintf(event->detail_name, sizeof(event->detail_name), "%s",
				 audio_state_name(previous_state));
	event_count++;
	spin_unlock(event_lock, owner);
}

void cdc_diagnostics_record_lcd_time(uint32_t duration_us)
{
	if (!COM_PORT_OUTPUT_ENABLE || !session_active || duration_us == 0u)
		return;

	lcd_timing.total_us += duration_us;
	lcd_timing.count++;
	if (duration_us > lcd_timing.max_us)
		lcd_timing.max_us = duration_us;
}

void cdc_diagnostics_record_fft_time(uint32_t duration_us)
{
	if (!COM_PORT_OUTPUT_ENABLE || !session_active || event_lock == NULL || duration_us == 0u)
		return;

	const uint32_t owner = spin_lock_blocking(event_lock);
	fft_timing.total_us += duration_us;
	fft_timing.count++;
	if (duration_us > fft_timing.max_us)
		fft_timing.max_us = duration_us;
	spin_unlock(event_lock, owner);
}

void cdc_diagnostics_task(void)
{
	if (!COM_PORT_OUTPUT_ENABLE)
		return;

	if (!tud_cdc_connected())
	{
		if (session_active || session_header_pos != 0)
			tud_cdc_write_clear();
		if (session_active)
		{
			session_active = false;
			reset_event_queue();
		}
		session_header_pos = 0;
		return;
	}

	const uint32_t input_available = tud_cdc_available();
	if (input_available != 0)
	{
		uint8_t discard[32];
		const uint32_t discard_length = input_available < sizeof(discard)
			? input_available : sizeof(discard);
		tud_cdc_read(discard, discard_length);
	}

	const uint32_t now = time_us_32();
	if (!session_active && !start_session(now))
		return;

	sample_ring_level(now);
	if ((int32_t)(now - next_sample_us) >= 0)
	{
		if (!write_sample(now))
			row_drop_count++;
	}
	write_pending_anomalies(now);
}

#else

void cdc_diagnostics_init(void)
{
}

bool cdc_diagnostics_is_connected(void)
{
	return false;
}

bool cdc_diagnostics_is_active(void)
{
	return false;
}

void cdc_diagnostics_log_anomaly(uint8_t code, const char *detail_name, uint32_t detail_value)
{
	(void)code;
	(void)detail_name;
	(void)detail_value;
}

void cdc_diagnostics_log_state_change(usb_audio_state_t previous_state,
									 usb_audio_state_t state, bool session_start)
{
	(void)previous_state;
	(void)state;
	(void)session_start;
}

void cdc_diagnostics_record_lcd_time(uint32_t duration_us)
{
	(void)duration_us;
}

void cdc_diagnostics_record_fft_time(uint32_t duration_us)
{
	(void)duration_us;
}

void cdc_diagnostics_task(void)
{
}

#endif
