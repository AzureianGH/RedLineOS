#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>

static int digit_val(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int vsscanf(const char* str, const char* fmt, va_list ap)
{
    const char* s = str;
    int assigned = 0;

    while (*fmt)
    {
        /* Skip whitespace in format */
        if (isspace(*fmt))
        {
            while (isspace(*s)) s++;
            fmt++;
            continue;
        }

        /* Literal match */
        if (*fmt != '%')
        {
            if (*s != *fmt)
                break;
            s++;
            fmt++;
            continue;
        }

        /* Conversion */
        fmt++; /* skip % */

        int suppress = 0;
        int width = 0;
        int length = 0;

        /* Assignment suppression */
        if (*fmt == '*')
        {
            suppress = 1;
            fmt++;
        }

        /* Width */
        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Length modifier */
        if (*fmt == 'h')
        {
            fmt++;
            if (*fmt == 'h') { length = 2; fmt++; } /* hh */
            else length = 1; /* h */
        }
        else if (*fmt == 'l')
        {
            fmt++;
            if (*fmt == 'l') { length = 4; fmt++; } /* ll */
            else length = 3; /* l */
        }

        char conv = *fmt++;
        if (!conv)
            break;

        /* Skip leading whitespace for most conversions */
        if (conv != 'c' && conv != '%')
            while (isspace(*s)) s++;

        /* %% */
        if (conv == '%')
        {
            if (*s != '%')
                break;
            s++;
            continue;
        }

        /* %c */
        if (conv == 'c')
        {
            int count = width ? width : 1;
            if (!s[0])
                break;

            if (!suppress)
            {
                char* out = va_arg(ap, char*);
                for (int i = 0; i < count; i++)
                {
                    if (!s[i]) break;
                    out[i] = s[i];
                }
                assigned++;
            }
            s += count;
            continue;
        }

        /* %s */
        if (conv == 's')
        {
            if (!*s)
                break;

            char* out = suppress ? NULL : va_arg(ap, char*);
            int n = 0;

            while (*s && !isspace(*s) && (!width || n < width))
            {
                if (!suppress)
                    out[n] = *s;
                s++;
                n++;
            }

            if (!suppress)
            {
                out[n] = '\0';
                assigned++;
            }
            continue;
        }

        /* Integer conversions */
        int base = 10;
        int is_signed = 0;

        if (conv == 'd') { base = 10; is_signed = 1; }
        else if (conv == 'u') { base = 10; }
        else if (conv == 'x') { base = 16; }
        else if (conv == 'o') { base = 8; }
        else
            break;

        int sign = 1;
        uint64_t val = 0;
        int consumed = 0;

        if (is_signed && (*s == '+' || *s == '-'))
        {
            if (*s == '-') sign = -1;
            s++;
            consumed++;
        }

        while (*s && (!width || consumed < width))
        {
            int d = digit_val(*s);
            if (d < 0 || d >= base)
                break;
            val = val * base + d;
            s++;
            consumed++;
        }

        if (consumed == 0)
            break;

        if (!suppress)
        {
            if (is_signed)
            {
                int64_t sval = (int64_t)val * sign;
                if (length == 1)      *va_arg(ap, short*) = (short)sval;
                else if (length == 2) *va_arg(ap, signed char*) = (signed char)sval;
                else if (length == 3) *va_arg(ap, long*) = (long)sval;
                else if (length == 4) *va_arg(ap, long long*) = (long long)sval;
                else                  *va_arg(ap, int*) = (int)sval;
            }
            else
            {
                if (length == 1)      *va_arg(ap, unsigned short*) = (unsigned short)val;
                else if (length == 2) *va_arg(ap, unsigned char*) = (unsigned char)val;
                else if (length == 3) *va_arg(ap, unsigned long*) = (unsigned long)val;
                else if (length == 4) *va_arg(ap, unsigned long long*) = (unsigned long long)val;
                else                  *va_arg(ap, unsigned int*) = (unsigned int)val;
            }
            assigned++;
        }
    }

    return assigned;
}
