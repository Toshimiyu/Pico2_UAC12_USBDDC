/*
 * Copyright (c) 2026 Toshimiyu
 *
 * Released under the MIT license
 * https://opensource.org/licenses/mit-license.php
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "hardware/vreg.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/regs/intctrl.h"
#include "common.h"
#include "usb_device_control.h"
#include "transmit_to_dac.h"
#include "ringbuffer.h"
#include "display_rta.h"
#include "spectrum.h"
#include "debug_gauge.h"

// 処理タイミング制御用
#define MILLISEC50 (500000 / TIMER0_US)

// タイマー割り込み
struct repeating_timer timer0; // Periodic stream housekeeping

// ring buffer
RINGBUFFER buffer_audio;

// Audio State
AUDIO_STATE audio_state;
uint32_t now_playing = 0;
static uint32_t now_playing_old = 0;
static bool is_cleared_buffer = false;

// 出力開始時間
volatile absolute_time_t time_start_output;

bool __not_in_flash_func(core0_timer_callback)(struct repeating_timer *t);
static void display_service(void);

// TinyUSB must be polled during startup animation; keep servicing USB while waiting.
static void boot_wait_ms(uint32_t ms)
{
	absolute_time_t end = make_timeout_time_ms(ms);
	while (!time_reached(end))
	{
		usb_audio_task();
		display_service();
		sleep_us(1);
	}
}


// 前回表示したオーディオフォーマット保持用 (追加)
static uint32_t last_display_freq = 0;
static uint8_t last_display_bit_depth = 0;
extern volatile uint32_t audio_last_data_us;
extern volatile bool audio_data_seen;

// Core1メイン
extern void core1_main();

// ミュート解除タイミング確認用
extern volatile bool enable_output;

static void display_service(void)
{
	const display_page_t previous_page = display_rta_get_page();
	const display_button_action_t button_action = display_rta_process_buttons();
	switch (button_action)
	{
	case DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_3:
		debug_gauge_set_reset_countdown(3);
		break;
	case DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_2:
		debug_gauge_set_reset_countdown(2);
		break;
	case DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_1:
		debug_gauge_set_reset_countdown(1);
		break;
	case DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_CLEARED:
		debug_gauge_set_reset_countdown(0);
		break;
	case DISPLAY_BUTTON_ACTION_RESET_DIAGNOSTICS:
		debug_gauge_reset();
		break;
	default:
		break;
	}
	const display_page_t requested_page = display_rta_get_page();
	if (requested_page != previous_page)
	{
		usb_audio_set_diagnostics_enabled(requested_page == DISPLAY_PAGE_DIAGNOSTICS);
	}

	if (display_rta_page_service())
	{
		const display_page_t page = display_rta_get_page();
		if (page == DISPLAY_PAGE_DIAGNOSTICS)
		{
			debug_gauge_init();
		}
		else if (page == DISPLAY_PAGE_PLAYBACK)
		{
			display_rta_set_format(audio_state.bit_depth, (float)audio_state.freq / 1000.0f);
		}
	}

	if (!display_rta_page_ready())
	{
		return;
	}

	const display_page_t page = display_rta_get_page();

	if (page == DISPLAY_PAGE_PLAYBACK)
	{
		display_rta_render_service();
	}
	else if (page == DISPLAY_PAGE_DIAGNOSTICS)
	{
		debug_gauge_service();
	}
}

void cancel_timer0(void)
{
	cancel_repeating_timer(&timer0);
}

void restart_timer0(void)
{
	add_repeating_timer_us(-TIMER0_US, core0_timer_callback, NULL, &timer0);
}

// Periodic playback housekeeping
bool __not_in_flash_func(core0_timer_callback)(struct repeating_timer *t)
{
	// volatile static uint32_t now_playing_old = 0;
	static volatile int count = 0;
	count++;
	if (count >= MILLISEC50)
	{
		// Clear queued PCM after playback has stopped
		if ((now_playing == now_playing_old) && (!is_cleared_buffer))
		{
			clear_ringbuffer(&buffer_audio);
			renew_clock();
			now_playing = 0;
			is_cleared_buffer = true;
		}
		else if (now_playing != now_playing_old)
		{
			is_cleared_buffer = false;
		}

		now_playing_old = now_playing;

		count = 0;
	}
	return true;
}

int main(void)
{
	// 外部電源有効化ピン
	if (USE_EXT_POWER_ENABLE)
	{
		gpio_init(EXT_POWER_ENABLE_PIN);
		gpio_set_dir(EXT_POWER_ENABLE_PIN, GPIO_OUT);
		gpio_put(EXT_POWER_ENABLE_PIN, false);
		sleep_us(BOOT_WAIT_TIME_US);
		gpio_put(EXT_POWER_ENABLE_PIN, true);
	}
	else
	{
		sleep_us(BOOT_WAIT_TIME_US);
	}

	// 動作電圧とクロックを引き上げる
	vreg_set_voltage(V_CORE_HI);
	sleep_ms(2);
	set_sys_clock_khz(SYS_CLOCK_KHZ_44K, true);
	sleep_us(1);

	// テストモード用ピンを有効化
	if (TEST_MODE)
	{
		gpio_init(TEST_PIN1);
		gpio_init(TEST_PIN2);
		gpio_set_dir(TEST_PIN1, GPIO_OUT);
		gpio_set_dir(TEST_PIN2, GPIO_OUT);
	}

	// 各種バッファ初期化
	initialize_ringbuffer(SIZE_AUDIO_RING_FRAMES * 2u, false, &buffer_audio);

	// オーディオステータス初期化
	audio_state.freq = AUDIO_INITIAL_FREQ;
	audio_state.bit_depth = 16;
	audio_state.mute = false;

	// Show the boot screen before the slower peripheral and DSP setup. The splash
	// routine returns immediately, so initialization continues with it visible.
	display_rta_hw_init();
	display_rta_clear_screen();
	
	display_rta_draw_splash_screen();

	// オンボードLED点灯用
	gpio_init(ONBOARD_LED_PIN);
	gpio_set_dir(ONBOARD_LED_PIN, GPIO_OUT);
	gpio_put(ONBOARD_LED_PIN, true);

	// DCDCの動作モード、trueでFPWM、falseでPFM
	gpio_init(DCDC_MODE_PIN);
	gpio_set_dir(DCDC_MODE_PIN, true);
	gpio_put(DCDC_MODE_PIN, true);

	// Periodic playback housekeeping.
	add_repeating_timer_us(-TIMER0_US, core0_timer_callback, NULL, &timer0);

	// Core1を起動する Core1ではI2S出力処理をしている
	multicore_launch_core1(core1_main);


	spectrum_init();
	spectrum_set_sample_rate(audio_state.freq);



	// USBを先に公開する
	usb_sound_card_init();
	boot_wait_ms(100);

	//起動画面描画
	display_rta_set_boot_stage(0);
	boot_wait_ms(1000);

	display_rta_set_boot_stage(1);
	boot_wait_ms(1000);

	display_rta_set_boot_stage(2);
	boot_wait_ms(1000);

	display_rta_set_boot_stage(3);
	boot_wait_ms(1000);

	display_rta_set_boot_stage(4);
	boot_wait_ms(1000);

	display_rta_set_boot_stage(5);
	boot_wait_ms(1000);

	// 起動後は再生画面を初期表示し、X/Yから起動画面・診断画面へ切り替える。
	display_rta_set_page(DISPLAY_PAGE_PLAYBACK);

	// watchdog_enable(50, 1);

	while (true)
	{
		// watchdog_update();

		// TinyUSB device task + direct PCM receive-to-I2S ring buffer + feedback/state management
		usb_audio_task();

		sleep_us(1);

		// --- ディスプレイ制御処理 ---
		
		// USBオーディオフォーマット（周波数 / ビット深度）に変更があれば表示を更新
    	if (audio_state.freq != last_display_freq || audio_state.bit_depth != last_display_bit_depth)
    	{
        last_display_freq = audio_state.freq;
        last_display_bit_depth = audio_state.bit_depth;
        
        // 周波数とビット深度を画面に反映
        display_rta_set_format(audio_state.bit_depth, (float)audio_state.freq / 1000.0f);
        spectrum_set_sample_rate(audio_state.freq);
    	}

	    // Monitor input inactivity on Core0; reset drawing is queued for the render service below.
	    static bool inactivity_screen_reset = false;
	    const bool audio_timed_out = audio_data_seen &&
	        ((uint32_t)(time_us_32() - audio_last_data_us) >= 200000u);
	    if (!audio_timed_out) inactivity_screen_reset = false;

	    // Hold the last meter/FFT frame long enough to observe short system test tones.
	    const bool display_inactive = audio_data_seen &&
	        ((uint32_t)(time_us_32() - audio_last_data_us) >= 1000000u);
	    if (display_inactive && !inactivity_screen_reset)
	    {
        display_rta_reset_levels();
	        inactivity_screen_reset = true;
	    }

	    // Only the active page is drawn; diagnostics no longer overlays the playback screen.
	    display_service();
	}
}
