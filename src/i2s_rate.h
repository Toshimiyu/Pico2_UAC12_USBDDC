#ifndef I2S_RATE_H_
#define I2S_RATE_H_

#include <stdint.h>

static inline float i2s_rate_from_clkdiv(uint32_t sys_hz, uint32_t clkdiv)
{
    uint32_t divider_q8 = clkdiv >> 8;
    if (divider_q8 == 0)
        divider_q8 = 65536u * 256u;
    return ((float)sys_hz * 2.0f) / (float)divider_q8;
}

#endif
