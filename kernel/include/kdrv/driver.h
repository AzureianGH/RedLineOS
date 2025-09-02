#pragma once
#include <stdint.h>
#include <stdbool.h>

// Simple kernel driver model (early skeleton)
// - Static drivers can register at init
// - Each driver declares name, type, init, optional late_init
// - Future: device tree / ACPI matching, probe/remove, power mgmt

typedef enum {
    KDRV_CLASS_TIMER,
    KDRV_CLASS_SERIAL,
    KDRV_CLASS_BLOCK,
    KDRV_CLASS_NET,
    KDRV_CLASS_INPUT,
    KDRV_CLASS_MISC,
} kdrv_class_t;

typedef struct kdrv kdrv_t;

typedef int  (*kdrv_init_t)(kdrv_t* drv);

typedef struct kdrv {
    const char*   name;
    kdrv_class_t  klass;
    kdrv_init_t   init;
    // reserved for future: probe/remove, suspend/resume, per-CPU, etc.
    void*         priv;
} kdrv_t;

// Registry
int  kdrv_register(kdrv_t* drv);
int  kdrv_init_all(void);

