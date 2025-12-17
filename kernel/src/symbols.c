#include <symbols.h>
#include <stddef.h>

// The ksym table and count are generated at build time from the linked binary.
extern const struct ksym ksym_table[];
extern const size_t ksym_count;

// Runtime slide applied when KASLR is enabled.
static uintptr_t symbol_slide = 0;

// Weak fallbacks so the first link pass can succeed before generation.
// Avoid initializers so the compiler cannot fold ksym_count to a constant zero.
__attribute__((weak)) const struct ksym ksym_table[1];
__attribute__((weak)) const size_t ksym_count;

void symbols_set_slide(uintptr_t slide) {
    symbol_slide = slide;
}

uintptr_t symbols_get_slide(void) {
    return symbol_slide;
}

const struct ksym *symbol_lookup(uintptr_t addr) {
    const uintptr_t unslid = addr - symbol_slide;
    if (ksym_count == 0) {
        return NULL;
    }

    size_t lo = 0;
    size_t hi = ksym_count - 1;
    const struct ksym *best = NULL;

    while (lo <= hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        const struct ksym *sym = &ksym_table[mid];

        if (unslid < sym->addr) {
            if (mid == 0) break;
            hi = mid - 1;
        } else {
            best = sym;      // sym->addr <= unslid
            lo = mid + 1;    // search right half for closer match
        }
    }

    return best;
}
