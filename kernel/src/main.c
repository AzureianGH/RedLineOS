// Core kernel entry; boot/ and libc/ pieces are split out for modularity.
// Keep main.c focused on high-level init and control flow.
#include <displaystandard.h>
#include <extendedint.h>
#include <stelloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <limine.h>
#include <stdio.h>
#include <boot.h>
#include <fb.h>
#include <libcinternal/libioP.h>
#include <palloc.h>
#include <lprintf.h>
#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <pic.h>
#include <pit.h>
#include <rtc.h>
#include <tsc.h>
#include <acpi.h>
#include <timer.h>
#include <lapic.h>
#include <ioapic.h>
#include <hpet.h>
#include <vmm.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>

// Externs for environment
extern int init_env();

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// Example shared atomic time variable
static _Atomic time_t shared_time;

static void halt_if(bool condition, const char *message) {
    if (condition) {
        if (message != NULL) {
            error_printf("%s\n", message);
        }
        hcf();
    }
}

static void verify_boot_requests(void) {
    info_printf("Verifying bootloader-provided structures...\n");
    bool invalid =
        LIMINE_BASE_REVISION_SUPPORTED == false             ||
        framebuffer_request.response == NULL                ||
        framebuffer_request.response->framebuffer_count < 1 ||
        hhdm_request.response == NULL                       ||
        hhdm_request.response->offset == 0                  ||
        memmap_request.response == NULL                     ||
        memmap_request.response->entry_count == 0;

    halt_if(invalid, "Bootloader did not supply required data; halting.");
    success_printf("Bootloader structures verified (fb count=%llu, memmap entries=%llu, hhdm=0x%llx).\n",
        (unsigned long long)framebuffer_request.response->framebuffer_count,
        (unsigned long long)memmap_request.response->entry_count,
        (unsigned long long)hhdm_request.response->offset);
}

static void init_descriptor_tables(void) {
    info_printf("Initializing descriptor tables (GDT/IDT)...\n");
    gdt_init();
    idt_init();
    exceptions_install_defaults();
    success_printf("Descriptor tables installed.\n");
}

static void init_display(void) {
    struct limine_framebuffer *lfb = framebuffer_request.response->framebuffers[0];
    info_printf("Initializing display: %ux%u %u bpp (pitch=%u)\n",
        lfb->width, lfb->height, lfb->bpp, lfb->pitch);
    displaystandard_init(&(struct fb){ .lfb = framebuffer_request.response->framebuffers[0] });
    set_debug_enabled(false);
    success_printf("Display initialized successfully.\n");
}

static void init_palloc(void) {
    info_printf("Initializing page allocator (palloc) with %llu memmap entries...\n",
        (unsigned long long)memmap_request.response->entry_count);
    palloc_init(memmap_request.response);
    success_printf("Page allocator initialized with %zu pages free.\n", palloc_get_free_page_count());
}

static void init_heap(void) {
    info_printf("Initializing stelloc heap allocator...\n");
    stelloc_init_heap();

    byte *stelloc_test = (byte *)malloc(1);
    halt_if(stelloc_test == NULL, "Heap allocator failed to return memory.");

    *stelloc_test = 0xAF;
    halt_if(*stelloc_test != 0xAF, "Stelloc heap allocator self-test failed.");
    free(stelloc_test);
    success_printf("Stelloc heap allocator initialized.\n");
}

static void init_pic(void) {
    info_printf("Remapping PIC...\n");
    pic_remap();
    pic_set_mask(0); // mask PIT
    pic_set_mask(1); // mask keyboard
    pic_set_mask(2); // cascade
    pic_set_mask(8); // mask RTC
    success_printf("PIC remapped and IRQs masked.\n");
}

static uint64_t calibrate_tsc(void) {
    info_printf("Calibrating TSC...\n");
    uint64_t tsc_hz = tsc_calibrate_hz(1193182u, 10);
    info_printf("TSC ~ %llu Hz\n", (unsigned long long)tsc_hz);
    return tsc_hz;
}

static void init_timers(uint64_t tsc_hz) {
    info_printf("Initializing timers (LAPIC/HPET/PIT fallback)...\n");
    bool ok = timer_init(1000, tsc_hz);
    halt_if(!ok, "Timer initialization failed.");

    switch (timer_source()) {
        case TIMER_SRC_LAPIC: success_printf("Using LAPIC timer at ~%u Hz\n", 1000u); break;
        case TIMER_SRC_HPET:  success_printf("Using HPET at ~%u Hz (periodic IRQ)\n", 1000u); break;
        case TIMER_SRC_PIT:   success_printf("Using PIT at 1000 Hz (legacy)\n"); break;
    }
}

static void init_environment(void) {
    info_printf("Initializing Environment...\n");
    init_env();

    int env_rc = setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
    halt_if(env_rc != 0, "Failed to set TZ environment variable.");
    success_printf("Environment initialized (TZ set).\n");
}

static void seed_shared_time(void) {
    time_t now = time(NULL);
    shared_time = now;
    printf("Current time: %s", ctime(&now));
    success_printf("Time seeded for shared clock.\n");
}

extern void kmain(void) {
    info_printf("=== Kernel startup begin ===\n");
    verify_boot_requests();

    init_descriptor_tables();
    init_display();
    init_palloc();
    init_heap();

    init_pic();
    uint64_t tsc_hz = calibrate_tsc();

    info_printf("Initializing ACPI...\n");
    acpi_init();

    init_timers(tsc_hz);

    // Defer enabling interrupts until after IDT, exception handlers, and timers are configured.
    idt_enable_interrupts();
    success_printf("Interrupts enabled.\n");

    init_environment();
    seed_shared_time();

    success_printf("Kernel initialization complete.\n");
    info_printf("=== Kernel startup end ===\n");

    while (true) {}
}
