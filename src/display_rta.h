/*
* Copyright (c) 2026 Toshimiyu 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#ifndef DISPLAY_RTA_H
#define DISPLAY_RTA_H

#include <stdbool.h>
#include <stdint.h>
#include "spectrum.h"

typedef enum
{
	DISPLAY_PAGE_BOOT = 0,
	DISPLAY_PAGE_PLAYBACK,
	DISPLAY_PAGE_DIAGNOSTICS
} display_page_t;

typedef enum
{
	DISPLAY_BUTTON_ACTION_NONE = 0,
	DISPLAY_BUTTON_ACTION_PAGE_CHANGED,
	DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_3,
	DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_2,
	DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_1,
	DISPLAY_BUTTON_ACTION_RESET_COUNTDOWN_CLEARED,
	DISPLAY_BUTTON_ACTION_RESET_DIAGNOSTICS
} display_button_action_t;

void display_rta_init(void);
void display_rta_hw_init(void);
void display_rta_clear_screen(void);
void display_rta_set_page(display_page_t page);
display_page_t display_rta_get_page(void);
bool display_rta_page_service(void);
bool display_rta_page_ready(void);
display_button_action_t display_rta_process_buttons(void);
void display_rta_splash_service(void);
void display_rta_update(void);
void display_rta_service(void);
void display_rta_render_service(void);
void display_rta_set_format(uint8_t bit_depth, float sample_rate_khz);

// 起動画面（Quad Heritage テーマ演出：10秒）
void display_rta_draw_splash_screen(void);
void display_rta_set_boot_stage(uint8_t stage);

// 実オーディオデータ入力用関数
void display_rta_set_levels(
    const uint8_t l_bands[SPECTRUM_BAND_COUNT],
    const uint8_t r_bands[SPECTRUM_BAND_COUNT],
    uint8_t l_peak,
    uint8_t r_peak);
void display_rta_set_bands(const uint8_t l_bands[SPECTRUM_BAND_COUNT], const uint8_t r_bands[SPECTRUM_BAND_COUNT]);
void display_rta_reset_levels(void);

// L/Rピークだけ更新
void display_rta_set_peak(
    uint8_t l_peak,
    uint8_t r_peak);

#endif // DISPLAY_RTA_H
