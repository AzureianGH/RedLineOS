#include <stdint.h>
#include <string.h>
#include <gdt.h>
#include <spinlock.h>
#include <stddef.h>

#pragma pack(push,1)
typedef struct {
    uint16_t limit;
    uint64_t base;
} gdtr_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} tss_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) gdt_tss_entry_t;
#pragma pack(pop)

// GDT layout:
// [0] null, [1] kernel code, [2] kernel data, [3] user data, [4] user code,
// [5-6] 64-bit TSS (descriptor spans 16 bytes)
typedef struct __attribute__((packed)) {
    gdt_entry_t entries[5];
    gdt_tss_entry_t tss_desc; // occupies two 8-byte slots (index 5 and 6)
} gdt_blob_t;

// Per-CPU TSS and GDT blobs (statically bounded for simplicity).
#define MAX_GDT_CPUS 256
static gdt_blob_t gdt_blobs[MAX_GDT_CPUS];
static tss_t tss_array[MAX_GDT_CPUS];
static spinlock_t gdt_lock = {0};
static int gdt_built = 0;

static void set_gdt_code_entry(gdt_entry_t* e, int dpl) {
    e->limit_low = 0;
    e->base_low = 0;
    e->base_mid = 0;
    e->access = 0x9A | ((dpl & 0x3) << 5); // present | exec | readable
    e->gran = 0x20 | 0x00 | 0x00; // Long mode (L=1), default operand size 0
    e->base_high = 0;
}

static void set_gdt_data_entry(gdt_entry_t* e, int dpl) {
    e->limit_low = 0;
    e->base_low = 0;
    e->base_mid = 0;
    e->access = 0x92 | ((dpl & 0x3) << 5); // present | writable
    e->gran = 0x00;
    e->base_high = 0;
}

static void set_tss_descriptor(gdt_tss_entry_t* d, uint64_t base, uint32_t limit) {
    d->limit_low = (uint16_t)(limit & 0xFFFF);
    d->base_low = (uint16_t)(base & 0xFFFF);
    d->base_mid = (uint8_t)((base >> 16) & 0xFF);
    d->access = 0x89; // present | type=0b1001 (64-bit TSS available)
    d->gran = (uint8_t)(((limit >> 16) & 0x0F));
    d->base_high = (uint8_t)((base >> 24) & 0xFF);
    d->base_upper = (uint32_t)(base >> 32);
    d->reserved = 0;
}

void gdt_set_kernel_rsp0(uint64_t rsp0) {
    // RSP0 is per-CPU; caller should have set the right CPU's TSS before calling.
    // For now, update the BSP slot (cpu 0) which is used for interrupts on the boot CPU.
    tss_array[0].rsp0 = rsp0;
}

extern void gdt_load_and_ltr(uint64_t gdtr_addr, uint16_t tss_selector);

void gdt_init(uint32_t cpu_index) {
    if (cpu_index >= MAX_GDT_CPUS) {
        cpu_index = 0; // fallback to BSP slot if we exceed static bound
    }

    spin_lock(&gdt_lock);
    if (!gdt_built) {
        gdt_built = 1;
    }

    // Build the per-CPU TSS and descriptor.
    tss_t *tss = &tss_array[cpu_index];
    gdt_blob_t *blob = &gdt_blobs[cpu_index];
    memset(tss, 0, sizeof(*tss));
    tss->iopb_offset = sizeof(*tss);

    memset(blob, 0, sizeof(*blob));
    set_gdt_code_entry(&blob->entries[1], 0); // kernel CS
    set_gdt_data_entry(&blob->entries[2], 0); // kernel DS/SS
    set_gdt_data_entry(&blob->entries[3], 3); // user DS/SS
    set_gdt_code_entry(&blob->entries[4], 3); // user CS
    set_tss_descriptor(&blob->tss_desc, (uint64_t)tss, sizeof(*tss)-1);

    spin_unlock(&gdt_lock);

    // Load GDT and TSS for this CPU (use this CPU's blob)
    gdtr_t gdtr_local;
    gdtr_local.base = (uint64_t)&blob->entries[0];
    gdtr_local.limit = sizeof(gdt_blob_t) - 1;
    gdt_load_and_ltr((uint64_t)&gdtr_local, GDT_SELECTOR_TSS);
}
