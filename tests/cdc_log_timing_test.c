#include <stdint.h>
#include "cdc_log_timing.h"

int main(void)
{
	uint64_t value = 0;
	if (!cdc_parse_time_sync("TIME,1760000000123000", &value) || value != 1760000000123000ull)
		return 1;
	if (!cdc_parse_time_sync("TIME,253402300799999999", &value) || value != 253402300799999999ull)
		return 2;
	const char *invalid[] = {
		"", "TIME,", "TIME,0", "TIME,-1", "TIME,12x", "OTHER,123",
		"TIME,253402300800000000", "TIME,18446744073709551616"
	};
	for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
		if (cdc_parse_time_sync(invalid[i], &value))
			return 3;

	uint32_t next_us = 0;
	uint32_t missed = 0;
	if (!cdc_fast_sample_due(0, &next_us, &missed) || next_us != 1000u || missed != 0u)
		return 4;
	if (cdc_fast_sample_due(500, &next_us, &missed))
		return 5;
	if (!cdc_fast_sample_due(1000, &next_us, &missed) || next_us != 2000u || missed != 0u)
		return 6;
	if (!cdc_fast_sample_due(4500, &next_us, &missed) || next_us != 5000u || missed != 2u)
		return 7;
	next_us = UINT32_MAX - 499u;
	missed = 0;
	if (!cdc_fast_sample_due(500, &next_us, &missed) || next_us != 1500u || missed != 1u)
		return 8;
	return 0;
}
