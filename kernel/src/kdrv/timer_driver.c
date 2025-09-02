#include <kdrv/driver.h>
#include <kdrv/ktimer.h>
#include <lprintf.h>

static int timerdrv_init(kdrv_t* drv) {
    (void)drv;
    info_printf("timerdrv: initializing kernel timer service\n");
    // Assume system already called ktimer_init from main; no-op here for now.
    return 0;
}

static kdrv_t g_timerdrv = {
    .name = "timer",
    .klass = KDRV_CLASS_TIMER,
    .init = timerdrv_init,
    .priv = 0,
};

__attribute__((constructor)) static void _reg(void) {
    kdrv_register(&g_timerdrv);
}
