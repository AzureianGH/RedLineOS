#pragma once
#include <stdint.h>
#include <time.h>

void rtc_init_periodic(int rate_power2); // rate 3..15
typedef void (*rtc_callback_t)(void);
int rtc_on_tick(rtc_callback_t cb);

// Read current RTC time as UNIX epoch seconds (UTC). Returns 0 on failure.
uint64_t rtc_read_epoch(void);
// Optional: read RTC into a struct tm (UTC). Returns 0 on success, -1 on failure.
int rtc_read_tm(struct tm* out);