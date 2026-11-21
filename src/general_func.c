/*
* Copyright (c) 2026 Toshimiyu 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#include <math.h>
#include "common.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "transmit_to_dac.h"

static void renew_cpu_clock(void)
{
	uint32_t target_khz;
	switch (audio_state.freq)
	{
	case 176400:
		target_khz = SYS_CLOCK_KHZ_176K4;
		break;
	case 88200:
		target_khz = SYS_CLOCK_KHZ_44K1_88K2;
		break;
	case 192000:
	case 96000:
	case 48000:
		target_khz = SYS_CLOCK_KHZ_48K;
		break;
	case 44100:
		target_khz = SYS_CLOCK_KHZ_44K1_88K2;
		break;
	default:
		target_khz = SYS_CLOCK_KHZ_44K1_88K2;
		break;
	}

	if (clock_get_hz(clk_sys) == target_khz * 1000u)
		return;

	vreg_set_voltage(V_CORE_HI);
	busy_wait_us(100);
	set_sys_clock_khz(target_khz, true);
	busy_wait_us(50);
}

void renew_clock(void)
{
	// Clear DMA.
	dma_stop_and_clear();

	// Stop PIO before changing its source clock.
	stop_i2s();
	renew_cpu_clock();

	// Reconfigure the PIO clock divider.
	reset_i2s_freq();

}


void volume_control(void)
{
	audio_state.vol_float = saturation_f32(powf(10.0f, (float)audio_state.acq_volume / (float)VOLUME_RESOLUTION / 20.0f), 1.0f, 0.0f);
}
