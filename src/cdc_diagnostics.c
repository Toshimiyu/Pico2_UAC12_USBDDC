#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "cdc_diagnostics.h"
#include "cdc_log_timing.h"
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
#define CDC_TRACE_QUEUE_SIZE 128u
#define CDC_TRACE_HISTORY_SIZE 128u
#define CDC_CSV_LINE_SIZE 640u
#define CDC_MEMORY_LINE_SIZE 128u
#define STRINGIFY_DETAIL(value) #value
#define STRINGIFY(value) STRINGIFY_DETAIL(value)
#define CDC_CSV_HEADER \
	"MS,FREQ,BIT_DEPTH,USB_MODE,HOST_RATE,FEEDBACK_RATE,HF_DIFF_NOW,RING_NOW,RING_MIN,RING_AVG,RING_MAX,MAX_HF_DIFF_SESSION,MAX_HF_DIFF_STREAM,STATE,UNDERFLOW_COUNT,USB_GAP_MAX_US,USBBUF_CAPACITY,USBBUF_FREE_NOW,USBBUF_FREE_MIN_STREAM,USBBUF_FREE_MAX_STREAM,USBBUF_FREE_MIN_PCT_STREAM,LCD_MAX_US,LCD_AVG_US,LCD_COUNT,FFT_MAX_US,FFT_AVG_US,FFT_COUNT,RATE_CHANGE_COUNT,STREAM_START_COUNT,STREAM_STOP_COUNT,LOOP0_LT1MS,LOOP0_LT2MS,LOOP0_LT5MS,LOOP0_GE5MS,LOOP1_LT1MS,LOOP1_LT2MS,LOOP1_LT5MS,LOOP1_GE5MS,FB_LEVEL,FB_INTEG_HZ,USB_OUT_AGE_US,USB_RESET_COUNT,USB_SUSPEND_COUNT,USB_RESUME_COUNT,DTR_ASSERT_COUNT,DTR_DEASSERT_COUNT\r\n"

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
	underflow_snapshot_t underflow;
} cdc_anomaly_t;

typedef struct
{
	uint32_t total_us;
	uint32_t max_us;
	uint32_t count;
} cdc_timing_window_t;

static spin_lock_t *event_lock;
static spin_lock_t *trace_lock;
static cdc_trace_t trace_queue[CDC_TRACE_QUEUE_SIZE];
static cdc_trace_t trace_history[CDC_TRACE_HISTORY_SIZE];
static uint32_t trace_head;
static uint32_t trace_count;
static uint32_t trace_sequence;
static uint32_t trace_dropped;
static uint32_t trace_history_head;
static uint32_t trace_history_count;
static cdc_trace_summary_t trace_summary;
static uint32_t silence_frames;
static bool silence_active;
static cdc_anomaly_t event_queue[CDC_EVENT_QUEUE_SIZE];
static uint8_t event_head;
static uint8_t event_count;
static uint32_t event_drop_count;
static uint32_t row_drop_count;
static usb_audio_event_cursor_t usb_event_cursor;
static bool last_cdc_connected;
static uint8_t session_header_pos;
static volatile bool session_active;
static uint32_t session_start_us;
static uint64_t session_start_uptime_us;
static uint32_t next_sample_us;
static uint32_t next_fast_us;
static uint32_t fast_missed;
static uint32_t fast_dropped;
static uint64_t clock_unix_us;
static uint64_t clock_uptime_us;
static bool clock_synced;
static bool clock_reply_pending;
static bool clock_error_pending;
static char clock_input[32];
static uint8_t clock_input_length;
static bool clock_input_overflow;
static uint32_t next_ring_average_us;
static uint64_t ring_average_sum;
static uint32_t ring_average_count;
static int32_t ring_min;
static int32_t ring_max;
static uint32_t max_hf_diff_session;
static uint32_t session_switch_start;
static uint32_t stream_start_count;
static uint32_t stream_stop_count;
static cdc_timing_window_t lcd_timing;
static cdc_timing_window_t fft_timing;
#if FB_EXPERIMENTAL_OFFSET_SEED
#define CDC_OFFSET_QUEUE_SIZE 16u
static bool cdc_write(const char *text, uint32_t length);
typedef struct
{
	uint64_t uptime_us;
	uint64_t age_us;
	uint32_t rate;
	bool seed;
	bool valid;
	bool have_stop;
	float saved_ppm;
	feedback_offset_result_t result;
} cdc_offset_event_t;
static cdc_offset_event_t offset_queue[CDC_OFFSET_QUEUE_SIZE];
static uint8_t offset_head;
static uint8_t offset_count;
static uint32_t offset_dropped;

static void enqueue_offset(cdc_offset_event_t event)
{
	if (!session_active)
		return;
	if (offset_count == CDC_OFFSET_QUEUE_SIZE)
	{
		offset_dropped++;
		return;
	}
	offset_queue[(offset_head + offset_count) % CDC_OFFSET_QUEUE_SIZE] = event;
	offset_count++;
}

void cdc_diagnostics_offset_seed(uint64_t uptime_us, uint32_t rate, bool valid,
	float saved_ppm, bool have_stop, uint64_t age_us)
{
	enqueue_offset((cdc_offset_event_t){.uptime_us = uptime_us, .rate = rate,
		.seed = true, .valid = valid, .saved_ppm = saved_ppm,
		.have_stop = have_stop, .age_us = age_us});
}

void cdc_diagnostics_offset_update(uint64_t uptime_us, uint32_t rate,
	const feedback_offset_result_t *result)
{
	enqueue_offset((cdc_offset_event_t){.uptime_us = uptime_us, .rate = rate,
		.result = *result});
}

static const char *offset_reason(feedback_offset_reason_t reason)
{
	switch (reason)
	{
	case OFFSET_ACCEPT_INITIAL: return "INITIAL";
	case OFFSET_ACCEPT_SMOOTHED: return "SMOOTHED";
	case OFFSET_REJECT_BOOST: return "BOOST";
	case OFFSET_REJECT_SATURATION: return "SATURATION";
	case OFFSET_REJECT_RANGE: return "RANGE";
	case OFFSET_REJECT_PENDING: return "PENDING";
	case OFFSET_REJECT_GAP: return "GAP";
	default: return "UNKNOWN";
	}
}

static void write_pending_offset(void)
{
	char line[256];
	while (offset_count != 0u)
	{
		const cdc_offset_event_t *event = &offset_queue[offset_head];
		int length;
		if (event->seed)
		{
			char age[24] = "";
			if (event->have_stop)
				snprintf(age, sizeof(age), "%llu", (unsigned long long)(event->age_us / 1000u));
			length = snprintf(line, sizeof(line), "OFFSET_SEED,%llu,%lu,%u,%.3f,%s\r\n",
				(unsigned long long)event->uptime_us, (unsigned long)event->rate,
				event->valid ? 1u : 0u, (double)event->saved_ppm, age);
		}
		else
		{
			const feedback_offset_result_t *r = &event->result;
			if (r->reason == OFFSET_REJECT_GAP)
				length = snprintf(line, sizeof(line),
					"OFFSET_UPDATE,%llu,%lu,,,,,GAP,%u,%.3f\r\n",
					(unsigned long long)event->uptime_us, (unsigned long)event->rate,
					r->saved_valid ? 1u : 0u, (double)r->saved_ppm);
			else
				length = snprintf(line, sizeof(line),
					"OFFSET_UPDATE,%llu,%lu,%.3f,%.3f,%.4f,%.2f,%s,%u,%.3f\r\n",
					(unsigned long long)event->uptime_us, (unsigned long)event->rate,
					(double)r->estimate_ppm, (double)r->correction_ppm,
					(double)r->slope_frames_s, (double)r->saturation_pct,
					offset_reason(r->reason), r->saved_valid ? 1u : 0u, (double)r->saved_ppm);
		}
		if (length < 0 || (size_t)length >= sizeof(line) || !cdc_write(line, (uint32_t)length))
			return;
		offset_head = (uint8_t)((offset_head + 1u) % CDC_OFFSET_QUEUE_SIZE);
		offset_count--;
	}
	if (offset_dropped != 0u)
	{
		const int length = snprintf(line, sizeof(line), "OFFSET_DROPPED,%lu\r\n",
			(unsigned long)offset_dropped);
		if (length > 0 && (size_t)length < sizeof(line) && cdc_write(line, (uint32_t)length))
			offset_dropped = 0;
	}
}
#endif

static const char *anomaly_name(uint8_t code)
{
	switch (code)
	{
	case 1: return "UNDERFLOW";
	case 2: return "OVERFLOW";
	case 3: return "FIFO_TRIM";
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

static void service_clock_input(void)
{
	uint8_t input[32];
	const uint32_t available = tud_cdc_available();
	const uint32_t count = tud_cdc_read(input, available < sizeof(input) ? available : sizeof(input));
	for (uint32_t i = 0; i < count; ++i)
	{
		const char c = (char)input[i];
		if (c == '\n')
		{
			clock_input[clock_input_length] = '\0';
			uint64_t unix_us;
			if (!clock_input_overflow && cdc_parse_time_sync(clock_input, &unix_us))
			{
				clock_uptime_us = time_us_64();
				clock_unix_us = unix_us;
				clock_synced = true;
				clock_reply_pending = true;
			}
			else
				clock_error_pending = true;
			clock_input_length = 0;
			clock_input_overflow = false;
		}
		else if (c != '\r')
		{
			if (clock_input_length < sizeof(clock_input) - 1u)
				clock_input[clock_input_length++] = c;
			else
				clock_input_overflow = true;
		}
	}
}

static void write_clock_reply(void)
{
	char line[96];
	int length;
	if (clock_error_pending)
		length = snprintf(line, sizeof(line), "#TIME_ERROR,INVALID_COMMAND\r\n");
	else if (clock_reply_pending)
		length = snprintf(line, sizeof(line), "#TIME_SYNC,%llu,%llu\r\n",
			(unsigned long long)clock_unix_us, (unsigned long long)clock_uptime_us);
	else
		return;
	if (length > 0 && (size_t)length < sizeof(line) && cdc_write(line, (uint32_t)length))
	{
		if (clock_error_pending)
			clock_error_pending = false;
		else
			clock_reply_pending = false;
	}
}

static void write_fast_sample(uint64_t uptime_us)
{
	const uint32_t now = (uint32_t)uptime_us;
	if (!cdc_fast_sample_due(now, &next_fast_us, &fast_missed))
		return;

	const usb_audio_state_t state = usb_audio_get_state();
	const usb_audio_stats_t *stats = usb_audio_get_stats();
	const uint32_t feedback = (state == USB_AUDIO_IDLE || state == USB_AUDIO_SWITCHING)
		? 0u : (uint32_t)((double)stats->feedback_hz * 65.536 + 0.5);
	const int32_t level_tenths = (int32_t)(usb_audio_get_feedback_level() * 10.0f + 0.5f);
	const int32_t integral_millihz = (int32_t)(usb_audio_get_feedback_integrator_hz() * 1000.0f);
	uint32_t out_age_us;
	char age[16] = "";
	if (usb_audio_get_out_age_us(&out_age_us))
		snprintf(age, sizeof(age), "%lu", (unsigned long)out_age_us);
	char unix_time[24] = "";
	if (clock_synced)
		snprintf(unix_time, sizeof(unix_time), "%llu",
			(unsigned long long)(clock_unix_us + uptime_us - clock_uptime_us));

	feedback_diagnostics_t feedback_diag;
	const bool have_feedback_diag = usb_audio_get_feedback_diagnostics(&feedback_diag);
	char diag_fields[112];
	const int diag_length = feedback_diag_format(diag_fields, sizeof(diag_fields),
		have_feedback_diag ? &feedback_diag : NULL, now);
	if (diag_length < 0 || (size_t)diag_length >= sizeof(diag_fields))
	{
		fast_dropped++;
		return;
	}

	char line[256];
	const int length = snprintf(line, sizeof(line),
		"FAST,%llu,%s,%u,%lu,%ld,%lu,%ld,%s,%lu,%lu,%s\r\n",
		(unsigned long long)uptime_us, unix_time, (unsigned)state,
		(unsigned long)audio_ring_total_frames(), (long)level_tenths,
		(unsigned long)feedback, (long)integral_millihz, age,
		(unsigned long)fast_missed, (unsigned long)fast_dropped, diag_fields);
	if (length < 0 || (size_t)length >= sizeof(line) || !cdc_write(line, (uint32_t)length))
		fast_dropped++;
}

// Caller holds trace_lock; no USB or audio-buffer operations run under this lock.
static void trace_push(cdc_trace_t event)
{
	if (trace_count == CDC_TRACE_QUEUE_SIZE)
	{
		trace_head = (trace_head + 1u) % CDC_TRACE_QUEUE_SIZE;
		trace_count--;
		trace_dropped++;
	}
	event.sequence = ++trace_sequence;
	trace_queue[(trace_head + trace_count) % CDC_TRACE_QUEUE_SIZE] = event;
	trace_count++;
	if (trace_history_count == CDC_TRACE_HISTORY_SIZE)
	{
		trace_history_head = (trace_history_head + 1u) % CDC_TRACE_HISTORY_SIZE;
		trace_history_count--;
		trace_summary.history_overwritten++;
	}
	trace_history[(trace_history_head + trace_history_count) % CDC_TRACE_HISTORY_SIZE] = event;
	trace_history_count++;
}

void cdc_diagnostics_silence_block(uint32_t frames)
{
	if (trace_lock == NULL)
		return;
	const uint64_t now = time_us_64();
	const uint32_t owner = spin_lock_blocking(trace_lock);
	if (!silence_active)
	{
		silence_active = true;
		silence_frames = 0;
		trace_summary.silence_episodes++;
		trace_push((cdc_trace_t){.uptime_us = now, .kind = TRACE_SILENCE_START});
	}
	silence_frames += frames;
	trace_summary.silence_frames += frames;
	spin_unlock(trace_lock, owner);
}

bool cdc_diagnostics_get_trace(uint32_t age, cdc_trace_t *event)
{
	if (trace_lock == NULL || event == NULL)
		return false;
	const uint32_t owner = spin_lock_blocking(trace_lock);
	const bool found = age < trace_history_count;
	if (found)
		*event = trace_history[(trace_history_head + trace_history_count - 1u - age) % CDC_TRACE_HISTORY_SIZE];
	spin_unlock(trace_lock, owner);
	return found;
}

void cdc_diagnostics_get_trace_summary(cdc_trace_summary_t *summary)
{
	if (trace_lock == NULL || summary == NULL)
		return;
	const uint32_t owner = spin_lock_blocking(trace_lock);
	*summary = trace_summary;
	spin_unlock(trace_lock, owner);
}

void cdc_diagnostics_silence_end(bool stopped)
{
	if (trace_lock == NULL)
		return;
	const uint64_t now = time_us_64();
	const uint32_t owner = spin_lock_blocking(trace_lock);
	if (silence_active)
	{
		trace_push((cdc_trace_t){
			.uptime_us = now,
			.kind = stopped ? TRACE_SILENCE_STOP : TRACE_SILENCE_RESUME,
			.silence_frames = silence_frames
		});
		silence_active = false;
	}
	spin_unlock(trace_lock, owner);
}

void cdc_diagnostics_trace_pcm(dma_pcm_trace_event_t event)
{
	if (trace_lock == NULL || event == DMA_PCM_TRACE_NONE)
		return;
	const cdc_trace_kind_t kind = event == DMA_PCM_TRACE_START ? TRACE_PCM_START :
		(event == DMA_PCM_TRACE_END ? TRACE_PCM_END : TRACE_PCM_ABORT);
	const cdc_trace_t trace = {
		.uptime_us = time_us_64(), .kind = kind, .rate = audio_state.freq
	};
	const uint32_t owner = spin_lock_blocking(trace_lock);
	trace_push(trace);
	spin_unlock(trace_lock, owner);
}

void cdc_diagnostics_trace_state(usb_audio_state_t previous_state, usb_audio_state_t state)
{
	if (trace_lock == NULL)
		return;
	const cdc_trace_t trace = {
		.uptime_us = time_us_64(), .kind = TRACE_STATE,
		.from = previous_state, .to = state, .rate = audio_state.freq,
		.ring = audio_ring_total_frames(), .dma_pool = dma_tx_pool_frames()
	};
	const uint32_t owner = spin_lock_blocking(trace_lock);
	trace_push(trace);
	spin_unlock(trace_lock, owner);
}

static void write_pending_trace(void)
{
	char line[160];
	cdc_trace_t event;
	uint32_t owner = spin_lock_blocking(trace_lock);
	const uint32_t dropped = trace_dropped;
	const bool have_event = trace_count != 0;
	if (have_event)
		event = trace_queue[trace_head];
	spin_unlock(trace_lock, owner);

	int length;
	if (dropped != 0)
		length = snprintf(line, sizeof(line), "TRACE_DROPPED,%lu\r\n", (unsigned long)dropped);
	else if (!have_event)
		return;
	else if (event.kind == TRACE_STATE)
		length = snprintf(line, sizeof(line), "TRACE,%llu,STATE,%s,%s,%lu,%lu,%lu,\r\n",
			(unsigned long long)event.uptime_us, audio_state_name(event.from),
			audio_state_name(event.to), (unsigned long)event.rate,
			(unsigned long)event.ring, (unsigned long)event.dma_pool);
	else if (event.kind == TRACE_PCM_START || event.kind == TRACE_PCM_END || event.kind == TRACE_PCM_ABORT)
		length = snprintf(line, sizeof(line), "TRACE,%llu,%s,,,%lu,,,\r\n",
			(unsigned long long)event.uptime_us,
			event.kind == TRACE_PCM_START ? "PCM_START" :
				(event.kind == TRACE_PCM_END ? "PCM_END" : "PCM_ABORT"),
			(unsigned long)event.rate);
	else
		length = snprintf(line, sizeof(line), "TRACE,%llu,%s,,,,,,%lu\r\n",
			(unsigned long long)event.uptime_us,
			event.kind == TRACE_SILENCE_START ? "SILENCE_START" :
				(event.kind == TRACE_SILENCE_RESUME ? "SILENCE_RESUME" : "SILENCE_STOP"),
			(unsigned long)event.silence_frames);
	if (length < 0 || (size_t)length >= sizeof(line) || !cdc_write(line, (uint32_t)length))
		return;

	owner = spin_lock_blocking(trace_lock);
	if (dropped != 0)
		trace_dropped -= dropped;
	else if (trace_count != 0 && trace_queue[trace_head].sequence == event.sequence)
	{
		trace_head = (trace_head + 1u) % CDC_TRACE_QUEUE_SIZE;
		trace_count--;
	}
	spin_unlock(trace_lock, owner);
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

static bool start_session(void)
{
	if (session_header_pos == 0)
	{
		session_start_uptime_us = time_us_64();
		session_start_us = (uint32_t)session_start_uptime_us;
	}
	const char *headers[] = {
		NULL,
		NULL,
		CDC_CSV_HEADER,
		"STATE,MS,FROM,TO,RATE,RING,DMA_POOL,USB_MODE\r\n",
		"ANOMALY,MS,CODE,DETAIL,USB_MODE,STATE,RATE,RING,DMA_POOL\r\n",
		NULL,
		"TRACE,UPTIME_US,EVENT,FROM,TO,RATE,RING,DMA_POOL,SILENCE_FRAMES\r\n",
		"USB_EVENT,UPTIME_US32,GENERATION,OVERWRITTEN_BEFORE,EVENT\r\n",
		"UNDERFLOW,MS,RING,DMA_POOL,RING_POOL,HOST_RATE,FEEDBACK_RATE,FB_LEVEL,FB_INTEG_HZ,USB_GAP_MAX_US,USB_OUT_AGE_US\r\n",
		"FAST,UPTIME_US,UNIX_US,STATE,RING,FB_LEVEL_X10,FEEDBACK_Q16,FB_INTEG_MILLIHZ,USB_OUT_AGE_US,MISSED,DROPPED,FB_DIAG_MODE,FB_DIAG_AGE_US,FB_P_MILLIHZ,FB_HF_MILLIHZ,FB_CORR_PPM_X10,FB_LIMIT_PPM_X10,FB_OUT_SAT,FB_I_SAT\r\n",
		NULL,
#if FB_EXPERIMENTAL_OFFSET_SEED
		"OFFSET_SEED,UPTIME_US,RATE,VALID,SAVED_PPM,STOP_AGE_MS\r\n",
		"OFFSET_UPDATE,UPTIME_US,RATE,ESTIMATE_PPM,CORRECTION_PPM,SLOPE_FRAMES_S,SATURATION_PCT,REASON,SAVED_VALID,SAVED_PPM\r\n",
		"OFFSET_DROPPED,COUNT\r\n",
#endif
	};
	char config_header[160];
	const int config_header_length = snprintf(config_header, sizeof(config_header),
		"#FW_VERSION=" FW_VERSION ",PREFILL_FRAMES=" STRINGIFY(USB_AUDIO_PREFILL_FRAMES)
		",PREFILL_MS=" STRINGIFY(TARGET_PREFILL_MS) ",PREFILL_MIN_FRAMES=%lu,USB_MODE=" USB_MODE_NAME "\r\n",
		(unsigned long)USB_AUDIO_STARTUP_PREFILL_FRAMES);
	if (config_header_length < 0 || (size_t)config_header_length >= sizeof(config_header))
		return false;
	headers[0] = config_header;
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
	char clock_header[64];
	const int clock_header_length = snprintf(clock_header, sizeof(clock_header),
		"#TRACE_SESSION_START_US=%llu\r\n", (unsigned long long)session_start_uptime_us);
	if (clock_header_length < 0 || (size_t)clock_header_length >= sizeof(clock_header))
		return false;
	headers[5] = clock_header;
	char feedback_header[512];
	const feedback_diag_config_t feedback_config = usb_audio_get_feedback_config();
	const int feedback_header_length =
		feedback_diag_format_config(feedback_header, sizeof(feedback_header), &feedback_config);
	if (feedback_header_length < 0 || (size_t)feedback_header_length >= sizeof(feedback_header))
		return false;
	headers[10] = feedback_header;
	while (session_header_pos < sizeof(headers) / sizeof(headers[0]))
	{
		const char *header = headers[session_header_pos];
		if (!cdc_write(header, (uint32_t)strlen(header)))
			return false;
		session_header_pos++;
	}

	reset_event_queue();
#if FB_EXPERIMENTAL_OFFSET_SEED
	offset_head = 0;
	offset_count = 0;
	offset_dropped = 0;
#endif
	memset(&usb_event_cursor, 0, sizeof(usb_event_cursor));
	row_drop_count = 0;
	const uint32_t now = time_us_32();
	next_sample_us = now;
	next_fast_us = now;
	fast_missed = 0;
	fast_dropped = 0;
	next_ring_average_us = now;
	ring_average_sum = 0;
	ring_average_count = 0;
	ring_min = INT32_MAX;
	ring_max = 0;
	max_hf_diff_session = 0;
	const usb_audio_stats_t *stats = usb_audio_get_stats();
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
	if (stats->switch_count < session_switch_start)
		session_switch_start = 0;
	const usb_audio_usb_counts_t usb_counts = usb_audio_get_usb_counts();
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
		",%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
		(unsigned long)stats->underflow_boot_count,
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

	uint32_t out_age_us;
	char out_age_text[16] = "";
	if (usb_audio_get_out_age_us(&out_age_us))
		snprintf(out_age_text, sizeof(out_age_text), "%lu", (unsigned long)out_age_us);
	const int detail_length = snprintf(line + written, sizeof(line) - (size_t)written,
		",%.1f,%.3f,%s,%lu,%lu,%lu,%lu,%lu\r\n", (double)usb_audio_get_feedback_level(),
		(double)usb_audio_get_feedback_integrator_hz(), out_age_text,
		(unsigned long)usb_counts.reset_count, (unsigned long)usb_counts.suspend_count,
		(unsigned long)usb_counts.resume_count, (unsigned long)usb_counts.dtr_assert_count,
		(unsigned long)usb_counts.dtr_deassert_count);
	if (detail_length < 0 || (size_t)detail_length >= sizeof(line) - (size_t)written)
		goto failed;
	written += detail_length;

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
		char out_age_text[16] = "";
		if (event.code == UINT8_MAX && event.underflow.usb_out_age_valid)
			snprintf(out_age_text, sizeof(out_age_text), "%lu",
					 (unsigned long)event.underflow.usb_out_age_us);
		const int written = event.code == UINT8_MAX
			? snprintf(line, sizeof(line),
				"UNDERFLOW,%lu,%ld,%ld,%ld,%lu,%lu,%.1f,%.3f,%lu,%s\r\n",
				(unsigned long)session_milliseconds(event.underflow.timestamp_us),
				(long)event.underflow.level, (long)event.underflow.dma_pool,
				(long)event.underflow.ring_pool, (unsigned long)event.underflow.host_rate_hz,
				(unsigned long)event.underflow.feedback_hz, (double)event.underflow.fb_level_f,
				(double)event.underflow.fb_integ_hz,
				(unsigned long)event.underflow.usb_gap_max_us, out_age_text)
			: event.code == 0
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

static void write_pending_usb_event(void)
{
	usb_audio_event_cursor_t next = usb_event_cursor;
	char event[8];
	uint32_t timestamp_us, lost;
	if (!usb_audio_get_next_event(&next, event, &timestamp_us, &lost))
		return;

	char line[96];
	const int length = snprintf(line, sizeof(line), "USB_EVENT,%lu,%lu,%lu,%s\r\n",
		(unsigned long)timestamp_us, (unsigned long)next.generation,
		(unsigned long)lost, event);
	if (length > 0 && (size_t)length < sizeof(line) && cdc_write(line, (uint32_t)length))
		usb_event_cursor = next;
}

void cdc_diagnostics_init(void)
{
	if (!COM_PORT_OUTPUT_ENABLE)
		return;

	event_lock = spin_lock_init(spin_lock_claim_unused(true));
	trace_lock = spin_lock_init(spin_lock_claim_unused(true));
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

void cdc_diagnostics_log_underflow(const underflow_snapshot_t *snapshot)
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
	event_queue[index].code = UINT8_MAX;
	event_queue[index].underflow = *snapshot;
	event_count++;
	spin_unlock(event_lock, owner);
}

void cdc_diagnostics_log_state_change(usb_audio_state_t previous_state,
									  usb_audio_state_t state, bool session_start)
{
	if (!COM_PORT_OUTPUT_ENABLE || event_lock == NULL)
		return;

	const uint32_t timestamp_us = time_us_32();
	const uint32_t sample_rate = audio_state.freq;
	const uint32_t ring_frames = (uint32_t)audio_ring_total_frames();
	const uint32_t dma_pool_frames = dma_tx_pool_frames();
	if (!session_active)
		return;
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

	const bool connected = tud_cdc_connected();
	if (connected != last_cdc_connected)
	{
		usb_audio_record_event('P', connected ? 1u : 0u, 0, 0, true, false);
		last_cdc_connected = connected;
	}

	if (!connected)
	{
		if (session_active || session_header_pos != 0)
			tud_cdc_write_clear();
		if (session_active)
		{
			session_active = false;
			reset_event_queue();
		}
		session_header_pos = 0;
		clock_synced = false;
		clock_reply_pending = false;
		clock_error_pending = false;
		clock_input_length = 0;
		clock_input_overflow = false;
		return;
	}

	service_clock_input();

	if (!session_active && !start_session())
		return;

	write_clock_reply();
	const uint32_t now = time_us_32();
	sample_ring_level(now);
	if ((int32_t)(now - next_sample_us) >= 0)
	{
		if (!write_sample(now))
			row_drop_count++;
	}
	write_pending_anomalies(now);
#if FB_EXPERIMENTAL_OFFSET_SEED
	write_pending_offset();
#endif
	write_pending_trace();
	write_pending_usb_event();
	write_fast_sample(time_us_64());
}

#else

#if FB_EXPERIMENTAL_OFFSET_SEED
void cdc_diagnostics_offset_seed(uint64_t uptime_us, uint32_t rate, bool valid,
	float saved_ppm, bool have_stop, uint64_t age_us)
{
	(void)uptime_us;
	(void)rate;
	(void)valid;
	(void)saved_ppm;
	(void)have_stop;
	(void)age_us;
}

void cdc_diagnostics_offset_update(uint64_t uptime_us, uint32_t rate,
	const feedback_offset_result_t *result)
{
	(void)uptime_us;
	(void)rate;
	(void)result;
}
#endif

bool cdc_diagnostics_get_trace(uint32_t age, cdc_trace_t *event)
{
	(void)age;
	(void)event;
	return false;
}

void cdc_diagnostics_get_trace_summary(cdc_trace_summary_t *summary)
{
	if (summary != NULL)
		*summary = (cdc_trace_summary_t){0};
}

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

void cdc_diagnostics_log_underflow(const underflow_snapshot_t *snapshot)
{
	(void)snapshot;
}

void cdc_diagnostics_log_state_change(usb_audio_state_t previous_state,
									 usb_audio_state_t state, bool session_start)
{
	(void)previous_state;
	(void)state;
	(void)session_start;
}

void cdc_diagnostics_silence_block(uint32_t frames)
{
	(void)frames;
}

void cdc_diagnostics_trace_state(usb_audio_state_t previous_state, usb_audio_state_t state)
{
	(void)previous_state;
	(void)state;
}

void cdc_diagnostics_silence_end(bool stopped)
{
	(void)stopped;
}

void cdc_diagnostics_trace_pcm(dma_pcm_trace_event_t event)
{
	(void)event;
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
