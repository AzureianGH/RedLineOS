#pragma once
void rtc_init_periodic(int rate_power2); // rate 3..15
typedef void (*rtc_callback_t)(void);
int rtc_on_tick(rtc_callback_t cb);