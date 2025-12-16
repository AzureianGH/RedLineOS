#include <acpi.h>
#include <limine.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lprintf.h>
#include <vmm.h>

extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_hhdm_request hhdm_request;

static const acpi_sdt_header_t* xsdt;
static const acpi_sdt_header_t* rsdt;
static const acpi_madt_t* madt_tbl;
static const acpi_hpet_t* hpet_tbl;

static uint8_t checksum8(const void* ptr, size_t len) {
    const uint8_t* p = (const uint8_t*)ptr;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) sum += p[i];
    return sum;
}

static bool sdt_sig_eq(const acpi_sdt_header_t* h, const char* sig) {
    return h->sig[0]==sig[0] && h->sig[1]==sig[1] && h->sig[2]==sig[2] && h->sig[3]==sig[3];
}

static inline const void* phys_to_virt_u64(uint64_t phys) {
    uint64_t off = hhdm_request.response ? hhdm_request.response->offset : 0;
    const void* v = (const void*)((uintptr_t)phys + (uintptr_t)off);
    debug_printf("ACPI: phys_to_virt_u64 phys=%#016llx + HHDM=%#016llx => %p\n",
                 (unsigned long long)phys, (unsigned long long)off, v);
    return v;
}

static inline const void* phys_to_virt_u32(uint32_t phys) {
    return phys_to_virt_u64((uint64_t)phys);
}

static const acpi_sdt_header_t* map_sdt(uint64_t phys) {
    if (!hhdm_request.response) return NULL;
    uint64_t hhdm = (uint64_t)hhdm_request.response->offset;
    uint64_t va_page = (phys + hhdm) & ~0xFFFULL;
    (void)vmm_map_page(va_page, va_page - hhdm, VMM_P_PRESENT | VMM_P_WRITABLE);
    const acpi_sdt_header_t* hdr = (const acpi_sdt_header_t*)phys_to_virt_u64(phys);
    if (!hdr) return NULL;
    uint32_t len = hdr->length;
    // Ensure every page spanned by the table is mapped before checksum/walks
    for (uint32_t off = 0; off < len; off += 0x1000u) {
        uint64_t pa = (phys + off) & ~0xFFFULL;
        uint64_t va = (phys + hhdm + off) & ~0xFFFULL;
        (void)vmm_map_page(va, pa, VMM_P_PRESENT | VMM_P_WRITABLE);
    }
    return hdr;
}

static const acpi_sdt_header_t* find_in_xsdt(const char* sig) {
    if (!xsdt) return NULL;
    uint32_t len = xsdt->length;
    uint32_t count = (len - sizeof(acpi_sdt_header_t)) / 8;
    const uint64_t* ents = (const uint64_t*)((const uint8_t*)xsdt + sizeof(acpi_sdt_header_t));
    debug_printf("ACPI: XSDT at %p, entries=%u\n", xsdt, (unsigned)count);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t phys = ents[i];
        const acpi_sdt_header_t* h = map_sdt(phys);
        debug_printf("ACPI: XSDT[%u] phys=%#016llx -> %p sig=%.4s\n", (unsigned)i,
                     (unsigned long long)ents[i], h, h ? h->sig : "null");
        if (h && sdt_sig_eq(h, sig) && checksum8(h, h->length) == 0) return h;
    }
    return NULL;
}

static const acpi_sdt_header_t* find_in_rsdt(const char* sig) {
    if (!rsdt) return NULL;
    uint32_t len = rsdt->length;
    uint32_t count = (len - sizeof(acpi_sdt_header_t)) / 4;
    const uint32_t* ents = (const uint32_t*)((const uint8_t*)rsdt + sizeof(acpi_sdt_header_t));
    debug_printf("ACPI: RSDT at %p, entries=%u\n", rsdt, (unsigned)count);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t phys = (uint64_t)ents[i];
        const acpi_sdt_header_t* h = map_sdt(phys);
        debug_printf("ACPI: RSDT[%u] phys=%#010x -> %p sig=%.4s\n", (unsigned)i,
                     (unsigned)ents[i], h, h ? h->sig : "null");
        if (h && sdt_sig_eq(h, sig) && checksum8(h, h->length) == 0) return h;
    }
    return NULL;
}

bool acpi_init(void) {
    if (!rsdp_request.response) return false;
    if (!hhdm_request.response || !hhdm_request.response->offset) return false;
    debug_printf("ACPI: RSDP phys=%#016llx HHDM=%#016llx\n",
                 (unsigned long long)rsdp_request.response->address,
                 (unsigned long long)hhdm_request.response->offset);
    debug_printf("ACPI: translating RSDP to virtual...\n");
    // Proactively map the RSDP page into HHDM if not present
    uint64_t rsdp_phys = (uint64_t)rsdp_request.response->address;
    uint64_t hhdm = (uint64_t)hhdm_request.response->offset;
    uint64_t rsdp_va_page = ((rsdp_phys + hhdm) & ~0xFFFULL);
    debug_printf("ACPI: ensuring map for RSDP phys=%#016llx at HHDM VA=%#016llx\n",
                 (unsigned long long)rsdp_phys, (unsigned long long)rsdp_va_page);
    (void)vmm_map_page(rsdp_va_page, rsdp_va_page - hhdm, VMM_P_PRESENT | VMM_P_WRITABLE);
    acpi_rsdp_t* rsdp = (acpi_rsdp_t*)phys_to_virt_u64(rsdp_phys);
    debug_printf("ACPI: RSDP virt ptr = %p\n", rsdp);
    if (!rsdp) { debug_printf("ACPI: RSDP virt is NULL\n"); return false; }
    if ((uintptr_t)rsdp < (uintptr_t)hhdm) {
        debug_printf("ACPI: ERROR virt ptr below HHDM! virt=%p hhdm=%#016llx\n", rsdp, (unsigned long long)hhdm);
    }
    debug_printf("ACPI: about to read RSDP->revision at %p+%zu\n", rsdp, (size_t)offsetof(acpi_rsdp_t, revision));
    uint8_t rsdp_rev = rsdp->revision;
    debug_printf("ACPI: RSDP read OK rev=%u\n", (unsigned)rsdp_rev);
    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        debug_printf("ACPI: translating XSDT phys=%#016llx\n", (unsigned long long)rsdp->xsdt_address);
        xsdt = map_sdt(rsdp->xsdt_address);
        debug_printf("ACPI: XSDT phys=%#016llx -> %p (about to checksum)\n",
                     (unsigned long long)rsdp->xsdt_address, xsdt);
        if (xsdt) {
            debug_printf("ACPI: XSDT length=%u first-bytes=%02x %02x %02x %02x\n",
                         (unsigned)xsdt->length,
                         ((const uint8_t*)xsdt)[0], ((const uint8_t*)xsdt)[1],
                         ((const uint8_t*)xsdt)[2], ((const uint8_t*)xsdt)[3]);
            if (checksum8(xsdt, xsdt->length) != 0) { debug_printf("ACPI: XSDT checksum bad\n"); xsdt = NULL; }
        }
    }
    if (rsdp->rsdt_address) {
        debug_printf("ACPI: translating RSDT phys=%#010x\n", (unsigned)rsdp->rsdt_address);
        rsdt = map_sdt((uint64_t)rsdp->rsdt_address);
        debug_printf("ACPI: RSDT phys=%#010x -> %p (about to checksum)\n", (unsigned)rsdp->rsdt_address, rsdt);
        if (rsdt) {
            debug_printf("ACPI: RSDT length=%u first-bytes=%02x %02x %02x %02x\n",
                         (unsigned)rsdt->length,
                         ((const uint8_t*)rsdt)[0], ((const uint8_t*)rsdt)[1],
                         ((const uint8_t*)rsdt)[2], ((const uint8_t*)rsdt)[3]);
            if (checksum8(rsdt, rsdt->length) != 0) { debug_printf("ACPI: RSDT checksum bad\n"); rsdt = NULL; }
        }
    }
    const acpi_sdt_header_t* madt_h = xsdt ? find_in_xsdt("APIC") : find_in_rsdt("APIC");
    if (madt_h) {
        madt_tbl = (const acpi_madt_t*)madt_h;
        debug_printf("ACPI: MADT @ %p len=%u rev=%u\n", madt_tbl, (unsigned)madt_tbl->hdr.length, (unsigned)madt_tbl->hdr.revision);
    } else {
        debug_printf("ACPI: MADT not found\n");
    }
    const acpi_sdt_header_t* hpet_h = xsdt ? find_in_xsdt("HPET") : find_in_rsdt("HPET");
    if (hpet_h) {
        hpet_tbl = (const acpi_hpet_t*)hpet_h;
        debug_printf("ACPI: HPET @ %p len=%u rev=%u\n", hpet_tbl, (unsigned)hpet_tbl->hdr.length, (unsigned)hpet_tbl->hdr.revision);
    } else {
        debug_printf("ACPI: HPET not found\n");
    }
    return true;
}

const acpi_madt_t* acpi_madt(void) { return madt_tbl; }
const acpi_hpet_t* acpi_hpet(void) { return hpet_tbl; }

uint64_t acpi_lapic_phys(void) {
    if (!madt_tbl) return 0;
    uint64_t addr = madt_tbl->lapic_addr;
    // Check for LAPIC Address Override (type 5)
    const uint8_t* p = (const uint8_t*)madt_tbl + sizeof(acpi_madt_t);
    const uint8_t* end = (const uint8_t*)madt_tbl + madt_tbl->hdr.length;
    debug_printf("ACPI: MADT LAPIC base (default) phys=%#016llx\n", (unsigned long long)addr);
    while (p + sizeof(acpi_madt_entry_hdr_t) <= end) {
        const acpi_madt_entry_hdr_t* h = (const acpi_madt_entry_hdr_t*)p;
        if (h->length == 0) break;
        debug_printf("ACPI: MADT entry type=%u len=%u at %p\n", (unsigned)h->type, (unsigned)h->length, h);
        if (h->type == ACPI_MADT_TYPE_LAPIC_ADDRESS_OVERRIDE && h->length >= sizeof(acpi_madt_lapic_addr_override_t)) {
            const acpi_madt_lapic_addr_override_t* lao = (const acpi_madt_lapic_addr_override_t*)p;
            addr = lao->lapic_addr;
            debug_printf("ACPI: MADT LAPIC override phys=%#016llx\n", (unsigned long long)addr);
        }
        p += h->length;
    }
    return addr;
}

bool acpi_get_first_ioapic(ioapic_info_t* out) {
    if (!madt_tbl || !out) return false;
    const uint8_t* p = (const uint8_t*)madt_tbl + sizeof(acpi_madt_t);
    const uint8_t* end = (const uint8_t*)madt_tbl + madt_tbl->hdr.length;
    while (p + sizeof(acpi_madt_entry_hdr_t) <= end) {
        const acpi_madt_entry_hdr_t* h = (const acpi_madt_entry_hdr_t*)p;
        if (h->length == 0) break;
        if (h->type == ACPI_MADT_TYPE_IOAPIC && h->length >= sizeof(acpi_madt_ioapic_t)) {
            const acpi_madt_ioapic_t* ioa = (const acpi_madt_ioapic_t*)p;
            out->phys_base = ioa->ioapic_addr;
            out->gsi_base = ioa->gsi_base;
            debug_printf("ACPI: IOAPIC phys=%#010x gsi_base=%u\n", (unsigned)ioa->ioapic_addr, (unsigned)ioa->gsi_base);
            return true;
        }
        p += h->length;
    }
    return false;
}
