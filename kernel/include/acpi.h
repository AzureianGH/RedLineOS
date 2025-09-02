#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char sig[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    // RSDP v2+
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

// MADT structures
typedef struct {
    acpi_sdt_header_t hdr;
    uint32_t lapic_addr;
    uint32_t flags;
    // followed by variable entries
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_hdr_t;

#define ACPI_MADT_TYPE_IOAPIC 1
#define ACPI_MADT_TYPE_INTERRUPT_OVERRIDE 2
#define ACPI_MADT_TYPE_LAPIC_ADDRESS_OVERRIDE 5

typedef struct {
    acpi_madt_entry_hdr_t h;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__((packed)) acpi_madt_ioapic_t;

typedef struct {
    acpi_madt_entry_hdr_t h;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed)) acpi_madt_int_override_t;

typedef struct {
    acpi_madt_entry_hdr_t h;
    uint16_t reserved;
    uint64_t lapic_addr;
} __attribute__((packed)) acpi_madt_lapic_addr_override_t;

// HPET table
typedef struct {
    acpi_sdt_header_t hdr;
    uint8_t hw_rev_id;
    uint8_t info; // bit0-4 comparator count, bit5 counter size, bit6-7 reserved
    uint16_t pci_vendor_id;
    struct {
        uint8_t address_space_id; // 0=system memory
        uint8_t register_bit_width;
        uint8_t register_bit_offset;
        uint8_t access_size;
        uint64_t address;
    } __attribute__((packed)) address;
    uint8_t hpet_number;
    uint16_t min_tick;
    uint8_t page_protection;
} __attribute__((packed)) acpi_hpet_t;

// Init and queries
bool acpi_init(void);
const acpi_madt_t* acpi_madt(void);
const acpi_hpet_t* acpi_hpet(void);
uint64_t acpi_lapic_phys(void);

typedef struct {
    uint64_t phys_base;
    uint32_t gsi_base;
} ioapic_info_t;

// Get first IOAPIC (simple UP case)
bool acpi_get_first_ioapic(ioapic_info_t* out);
