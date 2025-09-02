#include <stdint.h>
#include <string.h>
#include <gdt.h>

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

static gdt_blob_t gdt_blob;
static tss_t tss;
static gdtr_t gdtr;

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
    tss.rsp0 = rsp0;
}

extern void gdt_load_and_ltr(uint64_t gdtr_addr, uint16_t tss_selector);

void gdt_init(void) {
    // Zero TSS and set IO map to end of TSS (no IO bitmap)
    memset(&tss, 0, sizeof(tss));
    tss.iopb_offset = sizeof(tss);

    // Build GDT entries in a single contiguous blob (static memory)
    memset(&gdt_blob, 0, sizeof(gdt_blob));
    set_gdt_code_entry(&gdt_blob.entries[1], 0); // kernel CS
    set_gdt_data_entry(&gdt_blob.entries[2], 0); // kernel DS/SS
    set_gdt_data_entry(&gdt_blob.entries[3], 3); // user DS/SS
    set_gdt_code_entry(&gdt_blob.entries[4], 3); // user CS
    set_tss_descriptor(&gdt_blob.tss_desc, (uint64_t)&tss, sizeof(tss)-1);

    // Point GDTR to the blob start (first entry)
    gdtr.base = (uint64_t)&gdt_blob.entries[0];
    gdtr.limit = sizeof(gdt_blob) - 1;

    // Load GDT and TSS
    gdt_load_and_ltr((uint64_t)&gdtr, GDT_SELECTOR_TSS);
}
