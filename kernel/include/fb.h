#pragma once

#include <limine.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define COLOR_RGBA(r, g, b, a) \
    ((((uint32_t)(a) & 0xff) << 24) | \
    (((uint32_t)(b) & 0xff) << 16) | \
    (((uint32_t)(g) & 0xff) << 8)  | \
    (((uint32_t)(r) & 0xff) << 0))


#define COLOR_RGB(r, g, b) COLOR_RGBA(r, g, b, 0xff)

#define COLOR_WHITE COLOR_RGB(255, 255, 255)
#define COLOR_BLACK COLOR_RGB(0, 0, 0)
#define COLOR_RED   COLOR_RGB(255, 0, 0)
#define COLOR_GREEN COLOR_RGB(0, 255, 0)
#define COLOR_BLUE  COLOR_RGB(0, 0, 255)
#define COLOR_YELLOW COLOR_RGB(255, 255, 0)
#define COLOR_CYAN  COLOR_RGB(0, 255, 255)
#define COLOR_MAGENTA COLOR_RGB(255, 0, 255)
#define COLOR_GRAY  COLOR_RGB(128, 128, 128)
#define COLOR_DARK_GRAY COLOR_RGB(64, 64, 64)
#define COLOR_LIGHT_GRAY COLOR_RGB(192, 192, 192)
#define COLOR_ORANGE COLOR_RGB(255, 165, 0)
#define COLOR_PURPLE COLOR_RGB(128, 0, 128)

struct fb {
    struct limine_framebuffer *lfb;
};

static inline bool fb_is_valid(const struct fb *fb) {
    return fb && fb->lfb && fb->lfb->address && fb->lfb->width && fb->lfb->height;
}

static inline void fb_putpixel(struct fb *fb, uint32_t x, uint32_t y, uint32_t rgba) {
    if (!fb_is_valid(fb)) return;
    if (x >= fb->lfb->width || y >= fb->lfb->height) return;
    volatile uint32_t *ptr = (volatile uint32_t *)fb->lfb->address;
    ptr[y * (fb->lfb->pitch / 4) + x] = rgba;
}

static inline void fb_clear(struct fb *fb, uint32_t rgba) {
    if (!fb_is_valid(fb)) return;

    size_t count = fb->lfb->width * fb->lfb->height;

    __asm__ __volatile__ (
        "rep stosl"
        : /* no outputs */
        : "D"(fb->lfb->address),
          "a"(rgba),
          "c"(count)
        : "memory"
    );
}

