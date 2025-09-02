#pragma once
#include <stdint.h>

void pit_init(uint32_t hz);
typedef void (*pit_callback_t)(void);
int pit_on_tick(pit_callback_t cb); // up to a small number
