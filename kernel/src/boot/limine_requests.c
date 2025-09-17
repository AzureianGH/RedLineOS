#include <limine.h>
#include <stddef.h>

// Set the base revision to 3, latest described by the Limine spec (v3 API).
// Not static: referenced across translation units via LIMINE_BASE_REVISION_SUPPORTED.
__attribute__((used, section(".limine_requests")))
volatile LIMINE_BASE_REVISION(3);

// Framebuffer request (public, see header for extern).
__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL
};

// HHDM (Higher Half Direct Mapping) request.
__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL
};

// Memory map request.
__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL
};

// RSDP request for ACPI discovery.
__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0,
    .response = NULL
};

// SMP/MP request for bringing up application processors (APs).
__attribute__((used, section(".limine_requests")))
volatile struct LIMINE_MP(request) mp_request = {
    LIMINE_MP_REQUEST,
    .revision = 0,
    .response = NULL,
    .flags = 0
};

// Start/end markers for Limine requests.
__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;
