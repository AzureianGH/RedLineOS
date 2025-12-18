#include <stdlib.h>
#include <string.h>

static void swap_bytes(unsigned char *a, unsigned char *b, size_t size) {
    while (size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static void qsort_internal(unsigned char *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (nmemb < 2) return;
    size_t pivot_idx = nmemb / 2;
    swap_bytes(base, base + pivot_idx * size, size);
    size_t i = 1;
    size_t store = 1;
    for (; i < nmemb; ++i) {
        unsigned char *elem = base + i * size;
        if (compar(elem, base) < 0) {
            swap_bytes(elem, base + store * size, size);
            ++store;
        }
    }
    swap_bytes(base, base + (store - 1) * size, size);
    qsort_internal(base, store - 1, size, compar);
    qsort_internal(base + store * size, nmemb - store, size, compar);
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (!base || !compar || size == 0) return;
    qsort_internal((unsigned char *)base, nmemb, size, compar);
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    const unsigned char *b = (const unsigned char *)base;
    size_t low = 0, high = nmemb;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const void *elem = b + mid * size;
        int cmp = compar(key, elem);
        if (cmp == 0) return (void *)elem;
        if (cmp < 0) high = mid;
        else low = mid + 1;
    }
    return NULL;
}
