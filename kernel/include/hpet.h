#pragma once
#include <stdint.h>
#include <stdbool.h>

bool hpet_supported(void);
void hpet_init(void);
uint64_t hpet_counter_hz(void);
void hpet_sleep_ns(uint64_t ns);
void hpet_enable_periodic_irq(uint8_t comparator, uint64_t ns_interval, uint8_t vector);
// Convenience: enable comparator periodic mode and route its IRQ through IOAPIC to LAPIC vector.
bool hpet_enable_and_route_irq(uint8_t comparator, uint64_t ns_interval, uint8_t vector);
// Debug: number of HPET IRQs handled.
uint64_t hpet_get_irq_count(void);
bool hpet_route_irq_via_ioapic(uint8_t pin, uint8_t vector);

//ontick
typedef void (*hpet_irq_cb_t)(void);
int hpet_on_tick(hpet_irq_cb_t cb);
