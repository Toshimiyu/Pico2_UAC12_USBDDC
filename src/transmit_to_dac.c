/*
* Copyright (c) 2026 Toshimiyu 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#include "hardware/clocks.h"
#include "transmit_to_dac.h"
#include "usb_device_control.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/regs/dma.h"
#include "hardware/regs/pio.h"
#include "i2s_pio_interface.h"
#include "i2s_rate.h"
#include "cdc_diagnostics.h"
#include "idle_clock.h"
#include <stdatomic.h>
#include <string.h>
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "DMA fault handling requires lock-free atomic integers");
_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "DMA fault handling requires lock-free atomic booleans");
#if FB_EXPERIMENTAL_LEVEL_SEED
// A generation prevents an unfinished DMA fill from acknowledging a later stream.
static atomic_uint feedback_seed_generation;
static atomic_uint feedback_primed_generation;
static uint32_t feedback_last_signaled_generation;

uint32_t dma_tx_feedback_seed_arm(void)
{
    static uint32_t generation;
    if (++generation == 0)
        ++generation;
    atomic_store_explicit(&feedback_seed_generation, generation, memory_order_release);
    return generation;
}

bool dma_tx_feedback_seed_ready(uint32_t generation)
{
    return atomic_load_explicit(&feedback_primed_generation, memory_order_acquire) == generation;
}
#endif

static const PIO pio = pio0;
static const uint sm = 0;
static pio_sm_config sm_config;
static uint offset;

#define DMA_TX_SRAM_BANK_BYTES (64u * 1024u)
#define DMA_SILENCE_FRAMES (64u)
#define DMA_START_FADE_MS (5u)
#define DMA_STARTUP_SILENCE_US (10000000u)
#define DAC_ZERO_DETECT_FRAMES (1024u)
#define DAC_ZERO_SETTLE_MARGIN_US (1000u)

static uint32_t __attribute__((section(".dma_sram_bank7")))
    dma_tx_buffers[DEPTH_DMA_TX_BUFFER][SIZE_DMA_TX_BUF];

_Static_assert(sizeof(dma_tx_buffers) <= DMA_TX_SRAM_BANK_BYTES,
               "DMA buffers exceed SRAM bank 7");

static int dma_ch;
static volatile bool dma_running;
static volatile bool dma_running_pcm;
static atomic_uint dma_ahb_fault;
static atomic_bool dma_fault_quiescent;
static atomic_bool dma_startup_silence = true;
static atomic_bool dma_clock_ready = true;
static volatile bool dma_startup_clock_started;
static volatile uint32_t dma_startup_silence_since_us;
static bool dma_idle_clock_stopped;
static volatile bool dma_restart_in_progress;
static volatile bool dma_restart_clock_started;
static volatile uint32_t dma_restart_since_us;
static idle_clock_timer_t dma_idle_timer;
static bool dma_idle_silence_started;
static uint32_t dma_idle_silence_since_us;
static uint32_t dma_silence[DMA_SILENCE_FRAMES * 4u];
static volatile bool dma_was_silence = true;
static volatile bool dma_fade_out_in_flight;
static uint32_t dma_fade_in_total;
static uint32_t dma_fade_in_remaining;
static volatile int32_t dma_last_pcm_left;
static volatile int32_t dma_last_pcm_right;
DMA_TX_STRUCTURE dma_tx;

static void dma_tx_reset(void)
{
    dma_tx.wp = 0;
    dma_tx.rp = 0;
    dma_tx.using = 0;
    dma_tx.prev_write_length = 0;
    for (uint32_t i = 0; i < DEPTH_DMA_TX_BUFFER; i++)
    {
        dma_tx.data[i].tx_buf = dma_tx_buffers[i];
        dma_tx.data[i].tx_size = 0;
    }
    memset(dma_silence, 0, sizeof(dma_silence));
    dma_was_silence = true;
    dma_fade_out_in_flight = false;
    dma_fade_in_total = 0;
    dma_fade_in_remaining = 0;
    dma_last_pcm_left = 0;
    dma_last_pcm_right = 0;
}

volatile bool enable_output = false;

// Ver1.2 diagnostics -------------------------------------------------------------------------
DMA_DIAG dma_diag = {.armed = false, .level_min = INT32_MAX, .level_max = 0, .fetch_count = 0, .output_stop_count = 0,
                     .loop_prev_us = 0, .loop_max_us = 0, .loop_over_count = 0, .tail_block_count = 0};

void dma_diag_reset(void)
{
    dma_diag.level_min = INT32_MAX;
    dma_diag.level_max = 0;
    dma_diag.loop_max_us = 0;
    dma_diag.loop_over_count = 0;
    for (uint8_t i = 0; i < 4; ++i)
        dma_diag.loop_hist[i] = 0;
}

void dma_diag_reset_counters(void)
{
    dma_diag_reset();
    dma_diag.fetch_count = 0;
    dma_diag.output_stop_count = 0;
    dma_diag.loop_prev_us = 0;
    dma_diag.tail_block_count = 0;
}

float i2s_actual_sample_rate(void)
{
    return i2s_rate_from_clkdiv(clock_get_hz(clk_sys), pio->sm[sm].clkdiv);
}

// Blocks in use are the last dma_tx.using blocks written (oldest first). The oldest one may be partly
// played already, so this is an upper bound. Read from Core0 without locking: approximate by design.
uint32_t dma_tx_pool_frames(void)
{
    uint32_t n = dma_tx.using;
    if (n > DEPTH_DMA_TX_BUFFER)
        n = DEPTH_DMA_TX_BUFFER;
    uint32_t idx = (dma_tx.wp + DEPTH_DMA_TX_BUFFER - n) % DEPTH_DMA_TX_BUFFER;
    uint32_t frames = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        frames += dma_tx.data[idx].tx_size / 2u;
        idx = (idx + 1u) % DEPTH_DMA_TX_BUFFER;
    }
    return frames;
}

// PWM
static uint16_t pwm_slice;

void __not_in_flash_func(dma_tx_start)(void);
static void __not_in_flash_func(dma_tx_schedule)(void);
static bool dma_tail_seen = false; // lone-block episode already counted
static uint32_t output_start_us;

#if USB_CDC_ENABLED
static bool trace_pcm_active;

static void dma_trace_pcm(bool next_pcm, bool aborted)
{
    const dma_pcm_trace_event_t event = dma_pcm_trace_update(&trace_pcm_active, next_pcm, aborted);
    if (event != DMA_PCM_TRACE_NONE)
        cdc_diagnostics_trace_pcm(event);
}
#endif

bool calc_pwm_clkdiv_and_wrap_us(float target_period_us, uint16_t *wrap_out, float *clkdiv_out);
void set_pwm_isr_1(float period_us);
void set_period_pwm_us(float period_us);
void pwm_i2s_streaming_rate_change(void);
void __not_in_flash_func(pwm_wrap_handler)(void);

static bool __not_in_flash_func(dma_tx_check_ahb_errors)(void)
{
    const uint32_t error = dma_hw->ch[dma_ch].ctrl_trig &
        (DMA_CH0_CTRL_TRIG_READ_ERROR_BITS | DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS);
    if (error != 0u)
    {
        dma_channel_set_irq0_enabled((uint)dma_ch, false);
        atomic_store_explicit(&dma_ahb_fault, error, memory_order_release);
    }
    return error != 0u;
}

bool dma_tx_fault_pending(void)
{
    return atomic_load_explicit(&dma_fault_quiescent, memory_order_acquire);
}

bool dma_tx_startup_silence_active(void)
{
    return atomic_load_explicit(&dma_startup_silence, memory_order_acquire);
}

bool dma_tx_clock_ready(void)
{
    return atomic_load_explicit(&dma_clock_ready, memory_order_acquire);
}

bool dma_tx_is_silent(void)
{
    return dma_was_silence && !dma_fade_out_in_flight &&
        !dma_running_pcm && dma_tx.using == 0;
}

static void __not_in_flash_func(dma_tx_check_pio_stall)(uint32_t now_us)
{
    const uint32_t stall_mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    if ((pio->fdebug & stall_mask) == 0u)
        return;

    pio->fdebug = stall_mask;
    if (!enable_output || usb_audio_get_state() != USB_AUDIO_STREAMING ||
        (uint32_t)(now_us - output_start_us) < 10000u)
        return;

    const uint32_t fifo_level = pio_sm_get_tx_fifo_level(pio, sm);
    usb_audio_log_anomaly(9, (uint8_t)fifo_level, "TXFIFO", fifo_level);
}

static void __not_in_flash_func(dma_tx_schedule)(void)
{
    if (dma_running || atomic_load_explicit(&dma_ahb_fault, memory_order_acquire) != 0u)
        return;

    uint32_t *buffer;
    uint32_t words;
    if (dma_tx_startup_silence_active() || dma_restart_in_progress)
    {
        buffer = dma_silence;
        words = DMA_SILENCE_FRAMES * 2u;
        dma_running_pcm = false;
    }
    else if (dma_tx.using != 0)
    {
#if USB_CDC_ENABLED
        if (!dma_running_pcm)
            cdc_diagnostics_silence_end(false);
        dma_trace_pcm(true, false);
#endif
        const uint32_t rp = dma_tx.rp;
        buffer = dma_tx.data[rp].tx_buf;
        words = dma_tx.data[rp].tx_size;
        dma_tx.rp = (rp + 1u) % DEPTH_DMA_TX_BUFFER;
        dma_running_pcm = true;

        int32_t *samples = (int32_t *)buffer;
        const uint32_t frames = words / 2u;
        if (dma_was_silence)
        {
            dma_fade_in_total = (audio_state.freq * DMA_START_FADE_MS + 999u) / 1000u;
            if (dma_fade_in_total == 0u)
                dma_fade_in_total = 1u;
            dma_fade_in_remaining = dma_fade_in_total;
            dma_was_silence = false;
        }
        for (uint32_t frame = 0; frame < frames && dma_fade_in_remaining != 0u; frame++)
        {
            const uint32_t numerator = dma_fade_in_total - dma_fade_in_remaining + 1u;
            samples[2u * frame] =
                (int32_t)(((int64_t)samples[2u * frame] * numerator) / dma_fade_in_total);
            samples[2u * frame + 1u] =
                (int32_t)(((int64_t)samples[2u * frame + 1u] * numerator) / dma_fade_in_total);
            dma_fade_in_remaining--;
        }
        dma_last_pcm_left = samples[words - 2u];
        dma_last_pcm_right = samples[words - 1u];
    }
    else
    {
        buffer = dma_silence;
        words = DMA_SILENCE_FRAMES * 2u;
        if (!dma_was_silence)
        {
            for (uint32_t frame = 0; frame < DMA_SILENCE_FRAMES; frame++)
            {
                const int64_t numerator = DMA_SILENCE_FRAMES - frame - 1u;
                dma_silence[2u * frame] =
                    (uint32_t)(((int64_t)dma_last_pcm_left * numerator) / DMA_SILENCE_FRAMES);
                dma_silence[2u * frame + 1u] =
                    (uint32_t)(((int64_t)dma_last_pcm_right * numerator) / DMA_SILENCE_FRAMES);
            }
            words = DMA_SILENCE_FRAMES * 4u;
            dma_fade_out_in_flight = true;
            dma_last_pcm_left = 0;
            dma_last_pcm_right = 0;
            dma_was_silence = true;
            dma_fade_in_remaining = 0;
        }
        else
        {
            memset(dma_silence, 0, sizeof(dma_silence));
        }
#if USB_CDC_ENABLED
        const usb_audio_state_t state = usb_audio_get_state();
        const bool diagnostic_silence = enable_output &&
            (state == USB_AUDIO_STREAMING || state == USB_AUDIO_OVERFLOW_FATAL);
        dma_trace_pcm(false, false);
        if (diagnostic_silence)
        {
            cdc_diagnostics_silence_block(DMA_SILENCE_FRAMES);
        }
#endif
        dma_running_pcm = false;
    }

    dma_running = true;
    dma_channel_set_read_addr((uint)dma_ch, buffer, false);
    dma_channel_set_trans_count((uint)dma_ch, words, true);
}

static void __not_in_flash_func(dma_tx_irq_handler)(void)
{
    if (!dma_channel_get_irq0_status((uint)dma_ch))
        return;
    dma_channel_acknowledge_irq0((uint)dma_ch);
    if (dma_tx_check_ahb_errors())
        return;
    if (!dma_running)
        return;
    if (dma_running_pcm && dma_tx.using != 0)
        dma_tx.using--;
    else if (dma_fade_out_in_flight)
        dma_fade_out_in_flight = false;
    if (dma_tx_startup_silence_active() && !dma_startup_clock_started)
    {
        pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
        dma_startup_silence_since_us = time_us_32();
        dma_startup_clock_started = true;
    }
    if (dma_restart_in_progress && !dma_restart_clock_started)
    {
        pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
        dma_restart_since_us = time_us_32();
        dma_restart_clock_started = true;
    }
    dma_running = false;
    dma_tx_schedule();
#if USB_CDC_ENABLED
    if (!dma_running)
        dma_trace_pcm(false, false);
#endif
}

void reset_i2s_freq(void)
{
    I2S_freq_init(pio, sm, &sm_config, offset);
}

void stop_i2s(void)
{
    pio_sm_set_enabled(pio, sm, false);
}

void init_i2s_interface(void)
{
    if(I2S_SLEWRATE_FAST_ENABLE)
    {
        gpio_set_slew_rate(I2S_DATA_PIN, GPIO_SLEW_RATE_FAST);
        gpio_set_slew_rate(I2S_SIDESET_BASE, GPIO_SLEW_RATE_FAST);
        gpio_set_slew_rate(I2S_SIDESET_BASE + 1, GPIO_SLEW_RATE_FAST);
    }

    if(I2S_DRIVE_STRENGTH_2MA_ENABLE)
    {
        gpio_set_drive_strength(I2S_DATA_PIN, GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_drive_strength(I2S_SIDESET_BASE, GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_drive_strength(I2S_SIDESET_BASE + 1, GPIO_DRIVE_STRENGTH_2MA);
    }

    // Initialize PIO I2S.
    if(I2S_SIDESET_CHANGE)
    {
        I2S_32bit_inv_program_init(pio, sm, I2S_DATA_PIN, I2S_SIDESET_BASE, AUDIO_INITIAL_FREQ, &sm_config, &offset);
    }
    else
    {
        I2S_32bit_program_init(pio, sm, I2S_DATA_PIN, I2S_SIDESET_BASE, AUDIO_INITIAL_FREQ, &sm_config, &offset);
    }

    reset_i2s_freq();

    dma_ch = (int)dma_claim_unused_channel(true);
    dma_channel_config config = dma_channel_get_default_config((uint)dma_ch);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_dreq(&config, pio_get_dreq(pio, sm, true));

    dma_tx_reset();
    dma_running = false;
    dma_running_pcm = false;
    atomic_store_explicit(&dma_ahb_fault, 0u, memory_order_relaxed);
    atomic_store_explicit(&dma_fault_quiescent, false, memory_order_relaxed);
    dma_channel_configure((uint)dma_ch, &config,
                          &pio->txf[sm], dma_tx.data[0].tx_buf, 0, false);
    dma_hw->ints0 = 1u << dma_ch;
    irq_set_exclusive_handler(DMA_IRQ_0, dma_tx_irq_handler);
    dma_channel_set_irq0_enabled((uint)dma_ch, true);
    irq_set_enabled(DMA_IRQ_0, true);
    const uint32_t irq_state = save_and_disable_interrupts();
    dma_tx_schedule();
    restore_interrupts(irq_state);
}

// Notify the I2S transmit timing.
void __not_in_flash_func(pwm_wrap_handler)(void)
{
    pwm_clear_irq(pwm_slice); // Clear the PWM interrupt flag.
}

// Maximum number of stereo frames copied into one DMA buffer.
#define DMA_MAX_FRAMES (SIZE_DMA_TX_BUF / 2)

static volatile bool enable_output_prev = false;
extern volatile absolute_time_t time_start_output;

static uint32_t dma_zero_detect_us(void)
{
    return (DAC_ZERO_DETECT_FRAMES * 1000000u + audio_state.freq - 1u) /
        audio_state.freq + DAC_ZERO_SETTLE_MARGIN_US;
}

void __not_in_flash_func(dma_tx_start)(void)
{
    if (atomic_load_explicit(&dma_ahb_fault, memory_order_acquire) != 0u)
    {
        // Core0 may reset the pool only after the interrupted fill has finished.
        atomic_store_explicit(&dma_fault_quiescent, true, memory_order_release);
        return;
    }

    uint32_t now_us = time_us_32();
    const uint32_t rx_sequence = usb_audio_rx_sequence();
    const bool received = dma_idle_timer.initialized &&
        dma_idle_timer.sequence != rx_sequence;
    const bool idle_expired = idle_clock_expired(&dma_idle_timer, now_us, rx_sequence,
                                                  dma_tx_startup_silence_active());
    usb_audio_state_t state = usb_audio_get_state();

    if (dma_idle_clock_stopped)
    {
        if (!received && state != USB_AUDIO_SWITCHING)
            return;
        dma_idle_clock_stopped = false;
        dma_restart_in_progress = true;
        dma_restart_clock_started = false;
        dma_idle_silence_started = false;
        (void)idle_clock_expired(&dma_idle_timer, now_us, rx_sequence, true);
        reset_i2s_freq();
    }

    if (dma_tx_startup_silence_active())
    {
        if (!dma_startup_clock_started)
        {
            const uint32_t irq_state = save_and_disable_interrupts();
            dma_tx_schedule();
            restore_interrupts(irq_state);
            return;
        }
        const uint32_t stall_bit = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
        if ((pio->fdebug & stall_bit) != 0u)
        {
            pio->fdebug = stall_bit;
            dma_startup_silence_since_us = now_us;
        }
        else if ((uint32_t)(now_us - dma_startup_silence_since_us) >= DMA_STARTUP_SILENCE_US)
        {
            atomic_store_explicit(&dma_startup_silence, false, memory_order_release);
        }
        if (dma_tx_startup_silence_active())
        {
            const uint32_t irq_state = save_and_disable_interrupts();
            dma_tx_schedule();
            restore_interrupts(irq_state);
            return;
        }
    }

    if (dma_restart_in_progress)
    {
        if (!dma_restart_clock_started)
        {
            const uint32_t irq_state = save_and_disable_interrupts();
            dma_tx_schedule();
            restore_interrupts(irq_state);
            return;
        }
        now_us = time_us_32();
        const uint32_t stall_bit = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
        if ((pio->fdebug & stall_bit) != 0u)
        {
            pio->fdebug = stall_bit;
            dma_restart_since_us = now_us;
        }
        else if ((uint32_t)(now_us - dma_restart_since_us) >= dma_zero_detect_us())
        {
            dma_restart_in_progress = false;
            atomic_store_explicit(&dma_clock_ready, true, memory_order_release);
        }
        if (dma_restart_in_progress)
        {
            const uint32_t irq_state = save_and_disable_interrupts();
            dma_tx_schedule();
            restore_interrupts(irq_state);
            return;
        }
    }

    const uint32_t idle_irq_state = save_and_disable_interrupts();
    const bool quiet = !enable_output && dma_tx_is_silent() &&
        get_size_using(&buffer_audio) == 0;
    if (!quiet)
        dma_idle_silence_started = false;
    else if (!dma_idle_silence_started)
    {
        dma_idle_silence_started = true;
        dma_idle_silence_since_us = now_us;
    }
    if (quiet && idle_expired && state != USB_AUDIO_SWITCHING &&
        (uint32_t)(now_us - dma_idle_silence_since_us) >= dma_zero_detect_us() &&
        usb_audio_rx_sequence() == rx_sequence)
    {
        atomic_store_explicit(&dma_clock_ready, false, memory_order_release);
        dma_idle_clock_stopped = true;
        pio_sm_set_enabled(pio, sm, false);
        dma_stop_and_clear();
        pio_sm_set_pins_with_mask(pio, sm, 0,
            (1u << I2S_DATA_PIN) | (1u << I2S_SIDESET_BASE) | (1u << (I2S_SIDESET_BASE + 1u)));
        restore_interrupts(idle_irq_state);
        return;
    }
    restore_interrupts(idle_irq_state);

    // Loop-interval diagnostics: gap between two calls (this loop also runs the FFT)
    const uint32_t loop_now_us = time_us_32();
    static uint32_t anomaly_loop_prev_us;
    if (usb_audio_get_state() == USB_AUDIO_STREAMING && anomaly_loop_prev_us != 0)
    {
        const uint32_t gap_us = loop_now_us - anomaly_loop_prev_us;
        if (gap_us >= 5000u)
            usb_audio_log_anomaly(7, (uint8_t)(gap_us / 1000u), "GAP_US", gap_us);
    }
    anomaly_loop_prev_us = loop_now_us;

    if (dma_diag.armed && dma_diag.loop_prev_us != 0)
    {
        const uint32_t gap_us = loop_now_us - dma_diag.loop_prev_us;
        if (gap_us > dma_diag.loop_max_us)
            dma_diag.loop_max_us = gap_us;
        if (gap_us > LOOP1_WARN_US)
            dma_diag.loop_over_count++;
        const uint8_t hist_bin = gap_us < 1000u ? 0u :
            (gap_us < 2000u ? 1u : (gap_us < 5000u ? 2u : 3u));
        dma_diag.loop_hist[hist_bin]++;
    }
    dma_diag.loop_prev_us = dma_diag.armed ? loop_now_us : 0;

#if FB_EXPERIMENTAL_LEVEL_SEED
    const uint32_t seed_generation =
        atomic_load_explicit(&feedback_seed_generation, memory_order_acquire);
#endif
    const bool ring_paused = state == USB_AUDIO_BUFFERING || state == USB_AUDIO_UNDERFLOW_FATAL;
    if (ring_paused)
    {
        if (dma_tx_check_ahb_errors())
            return;
        const uint32_t irq_state = save_and_disable_interrupts();
        dma_tx_schedule();
        if (dma_tx_is_silent())
        {
            if (enable_output && dma_diag.armed)
                dma_diag.output_stop_count++;
            enable_output = false;
            enable_output_prev = false;
        }
        restore_interrupts(irq_state);
        return;
    }

    int32_t level = (int32_t)get_size_using(&buffer_audio) / 2;
    if ((state == USB_AUDIO_STREAMING && level > 0) ||
        (state != USB_AUDIO_STREAMING && level >= SIZE_AUDIO_PREFILL_FRAMES))
        enable_output = true;
    else if (level == 0 && dma_tx_is_silent() &&
             state != USB_AUDIO_STREAMING && state != USB_AUDIO_OVERFLOW_FATAL)
    {
        enable_output = false;
    }


    if (enable_output && !enable_output_prev)
    {
        output_start_us = loop_now_us;
        pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
        time_start_output = get_absolute_time();
    }
    if (enable_output_prev && !enable_output && dma_diag.armed)
        dma_diag.output_stop_count++;
    enable_output_prev = enable_output;

    if (!enable_output)
    {
        const uint32_t irq_state = save_and_disable_interrupts();
        dma_tx_schedule();
        restore_interrupts(irq_state);
        return;
    }

#if FB_EXPERIMENTAL_LEVEL_SEED
    bool queued_streaming_pcm = false;
#endif
    while (dma_tx.using < SIZE_DMA_TX_BUF_STACK)
    {
        const usb_audio_state_t fetch_state = usb_audio_get_state();
        if (fetch_state == USB_AUDIO_BUFFERING || fetch_state == USB_AUDIO_UNDERFLOW_FATAL)
            break;

        int32_t frames = (int32_t)get_size_using(&buffer_audio) / 2;
        if (frames <= 0)
            break;
        const int32_t level_before = frames; // Ring level just before this fetch
        if (frames > DMA_MAX_FRAMES)
            frames = DMA_MAX_FRAMES;

        const uint32_t wp = dma_tx.wp;
        uint32_t *tx = dma_tx.data[wp].tx_buf;
        if (ringbuf_read_array_spinlock((int32_t *)tx, (uint32_t)frames * 2u, &buffer_audio) < 0)
            break;

        dma_diag.fetch_count++;
        if (dma_diag.armed)
        {
            const int32_t level_after = (int32_t)get_size_using(&buffer_audio) / 2;
            if (level_before > dma_diag.level_max)
                dma_diag.level_max = level_before;
            if (level_after < dma_diag.level_min)
                dma_diag.level_min = level_after;
        }

        const uint32_t irq_state = save_and_disable_interrupts();
        dma_tx.data[wp].tx_size = (uint32_t)frames * 2u;
        dma_tx.wp = (wp + 1u) % DEPTH_DMA_TX_BUFFER;
        dma_tx.using++;
        restore_interrupts(irq_state);
#if FB_EXPERIMENTAL_LEVEL_SEED
        if (fetch_state == USB_AUDIO_STREAMING)
            queued_streaming_pcm = true;
#endif
    }

#if FB_EXPERIMENTAL_LEVEL_SEED
    if (queued_streaming_pcm && dma_tx.using >= SIZE_DMA_TX_BUF_STACK &&
        seed_generation != feedback_last_signaled_generation)
    {
        atomic_store_explicit(&feedback_primed_generation, seed_generation, memory_order_release);
        feedback_last_signaled_generation = seed_generation;
    }
#endif

    if (dma_tx.using == 1 && get_size_using(&buffer_audio) == 0)
    {
        if (!dma_tail_seen)
        {
            dma_tail_seen = true;
            if (dma_diag.armed)
                dma_diag.tail_block_count++;
        }
    }
    else
    {
        dma_tail_seen = false;
    }
    if (dma_tx_check_ahb_errors())
        return;
    dma_tx_schedule();
    dma_tx_check_pio_stall(time_us_32());
}

void dma_stop_and_clear(void)
{
    const uint32_t irq_state = save_and_disable_interrupts();
    dma_channel_set_irq0_enabled((uint)dma_ch, false);
    hw_clear_bits(&dma_hw->ch[dma_ch].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_abort((uint)dma_ch);
    dma_hw->intr = 1u << dma_ch;
    const uint32_t ctrl = dma_hw->ch[dma_ch].al1_ctrl;
    dma_hw->ch[dma_ch].al1_ctrl = ctrl |
        DMA_CH0_CTRL_TRIG_READ_ERROR_BITS | DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS;

#if USB_CDC_ENABLED
    cdc_diagnostics_silence_end(true);
    dma_trace_pcm(false, true);
#endif
    pio_sm_clear_fifos(pio0, 0);
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);

    dma_tx_reset();
    dma_running = false;
    dma_running_pcm = false;

    enable_output = false;
    enable_output_prev = false;
    hw_set_bits(&dma_hw->ch[dma_ch].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_set_irq0_enabled((uint)dma_ch, true);
    restore_interrupts(irq_state);
}

uint32_t dma_tx_discard_pending(void)
{
    uint32_t discarded_frames = 0;
    const uint32_t using = dma_tx.using;
    const uint32_t preserve = (dma_running && dma_running_pcm && using != 0u) ? 1u : 0u;
    uint32_t index = (dma_tx.wp + DEPTH_DMA_TX_BUFFER - using) % DEPTH_DMA_TX_BUFFER;
    for (uint32_t i = preserve; i < using; i++)
    {
        discarded_frames += dma_tx.data[index].tx_size / 2u;
        index = (index + 1u) % DEPTH_DMA_TX_BUFFER;
    }
    dma_tx.wp = dma_tx.rp;
    dma_tx.using = preserve;
    return discarded_frames;
}

uint8_t dma_tx_recover_fault(void)
{
    const uint32_t error = atomic_load_explicit(&dma_ahb_fault, memory_order_acquire);
    dma_stop_and_clear();
    if (dma_tx_startup_silence_active())
        dma_startup_clock_started = false;
    if (dma_restart_in_progress)
        dma_restart_clock_started = false;
    atomic_store_explicit(&dma_fault_quiescent, false, memory_order_relaxed);
    atomic_store_explicit(&dma_ahb_fault, 0u, memory_order_release);
    return (uint8_t)(((error & DMA_CH0_CTRL_TRIG_READ_ERROR_BITS) ? 1u : 0u) |
                     ((error & DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS) ? 2u : 0u));
}
