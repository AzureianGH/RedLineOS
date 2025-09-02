#include <displaystandard.h>
#include <flanterm.h>
#include <ftfb.h>

struct fb *global_fb;
struct flanterm_context *ft_ctx;

void displaystandard_init(struct fb *fb)
{
    global_fb = fb;
    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        fb->lfb->address, fb->lfb->width, fb->lfb->height, fb->lfb->pitch,
        fb->lfb->red_mask_size, fb->lfb->red_mask_shift,
        fb->lfb->green_mask_size, fb->lfb->green_mask_shift,
        fb->lfb->blue_mask_size, fb->lfb->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );
}

void displaystandard_putc(char c)
{
    if (global_fb != NULL && ft_ctx != NULL) {
        flanterm_write(ft_ctx, &c, 1);
    }
}