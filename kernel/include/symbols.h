#pragma once

#include <stdint.h>
#include <stddef.h>

struct ksym {
    uintptr_t addr;       // Symbol starting address
    const char *name;     // Symbol name string
};

// Look up the nearest symbol at or below the given address.
// Returns NULL if no symbol is known for the address.
const struct ksym *symbol_lookup(uintptr_t addr);

// Set or query the active KASLR slide (0 when KASLR is disabled).
void symbols_set_slide(uintptr_t slide);
uintptr_t symbols_get_slide(void);
