#include <kdrv/driver.h>
#include <stddef.h>

#define MAX_KDRIVERS 64
static kdrv_t* g_drivers[MAX_KDRIVERS];

int kdrv_register(kdrv_t* drv) {
    for (int i = 0; i < MAX_KDRIVERS; ++i) {
        if (!g_drivers[i]) { g_drivers[i] = drv; return 0; }
    }
    return -1;
}

int kdrv_init_all(void) {
    for (int i = 0; i < MAX_KDRIVERS; ++i) {
        if (g_drivers[i] && g_drivers[i]->init) {
            int rc = g_drivers[i]->init(g_drivers[i]);
            if (rc != 0) return rc;
        }
    }
    return 0;
}
