#include <stdlib.h>
#include <stddef.h>

static int is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int digit_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

static unsigned long long strto_ull_core(const char *nptr, char **endptr, int base, int *neg_out) {
    const char *s = nptr;
    while (is_space(*s)) ++s;
    int neg = 0;
    if (*s == '+' || *s == '-') {
        if (*s == '-') neg = 1;
        ++s;
    }
    if (base == 0) {
        if (s[0] == '0') {
            if (s[1] == 'x' || s[1] == 'X') { base = 16; s += 2; }
            else { base = 8; s += 1; }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            s += 2;
        }
    }
    unsigned long long acc = 0;
    int any = 0;
    for (;;) {
        int d = digit_val(*s);
        if (d < 0 || d >= base) break;
        any = 1;
        acc = acc * (unsigned)base + (unsigned)d;
        ++s;
    }
    if (endptr) {
        *endptr = (char *)(any ? s : nptr);
    }
    if (neg_out) *neg_out = neg;
    return neg ? (unsigned long long)(-(long long)acc) : acc;
}

long strtol(const char *nptr, char **endptr, int base) {
    int neg = 0;
    unsigned long long v = strto_ull_core(nptr, endptr, base, &neg);
    return neg ? -(long)v : (long)v;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    int neg = 0;
    unsigned long long v = strto_ull_core(nptr, endptr, base, &neg);
    return neg ? (unsigned long)(-(long long)v) : (unsigned long)v;
}

long long strtoll(const char *nptr, char **endptr, int base) {
    int neg = 0;
    unsigned long long v = strto_ull_core(nptr, endptr, base, &neg);
    return neg ? -(long long)v : (long long)v;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    int neg = 0;
    unsigned long long v = strto_ull_core(nptr, endptr, base, &neg);
    return neg ? (unsigned long long)(-(long long)v) : v;
}

int atoi(const char *nptr) { return (int)strtol(nptr, NULL, 10); }
long atol(const char *nptr) { return strtol(nptr, NULL, 10); }
long long atoll(const char *nptr) { return strtoll(nptr, NULL, 10); }

int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }
