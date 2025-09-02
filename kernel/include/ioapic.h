#pragma once
#include <stdint.h>
#include <stdbool.h>

bool ioapic_supported(void);
void ioapic_init(void);
void ioapic_mask_irq(uint8_t gsi);
void ioapic_unmask_irq(uint8_t gsi);
void ioapic_route_irq(uint8_t gsi, uint8_t vector);
// Helper: retrieve the IOAPIC's GSI base (from ACPI MADT)
uint32_t ioapic_get_gsi_base(void);
