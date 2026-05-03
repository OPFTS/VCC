/*
 * vero_stdio.c — VeroCC stdio
 * Implements printf/snprintf/puts/putchar/write using only syscalls.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/syscall.h"
#include "verolibc.h"

/* ── Integer to string conversion ───────────────────────────── */
static int itoa_buf(char *buf, size_t bsz, unsigned long long val,
                    int base, int upper, int width, int zero_pad,
                    int is_signed, int neg) {
    const char *dig_l = "0123456789abcdef";
    const char *dig_u = "0123456789ABCDEF";
    const char *dig   = upper ? dig_u : dig_l;

    char tmp[64]; int tlen = 0;
    if (val == 0) { tmp[tlen++] = '0'; }
    while (val) { tmp[tlen++] = dig[val % base]; val /= base; }

    int total = tlen + (neg ? 1 : 0);
    int pad   = (width > total) ? width - total : 0;

    int n = 0;
    if (!zero_pad) while (pad-- > 0) { if (n < (int)bsz-1) buf[n] = ' '; n++; }
    if (neg)       { if (n < (int)bsz-1) buf[n] = '-'; n++; }
    if (zero_pad)  while (pad-- > 0) { if (n < (int)bsz-1) buf[n] = '0'; n++; }
    for (int i = tlen-1; i >= 0; i--) {
        if (n < (int)bsz-1) buf[n] = tmp[i];
        n++;
    }
    buf[n < (int)bsz ? n : (int)bsz-1] = '\0';
    return n;
}

/* ── Core vsnprintf ───────────────────────────────────────────── */
int vero_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    int n = 0;
#define PUT(c) do { if ((size_t)n < size-1) buf[n] = (c); n++; } while(0)

    while (*fmt) {
        if (*fmt != '%') { PUT(*fmt++); continue; }
        fmt++;

        /* flags */
        int zero_pad=0, left=0, plus=0, space=0, alt=0;
        for (;;) {
            if      (*fmt=='0') { zero_pad=1; fmt++; }
            else if (*fmt=='-') { left=1;     fmt++; }
            else if (*fmt=='+') { plus=1;     fmt++; }
            else if (*fmt==' ') { space=1;    fmt++; }
            else if (*fmt=='#') { alt=1;      fmt++; }
            else break;
        }

        /* width */
        int width = 0;
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') width = width*10 + (*fmt++ - '0');

        /* precision */
        int prec = -1;
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') prec = prec*10 + (*fmt++ - '0');
        }

        /* length modifier */
        int lng = 0;
        if (*fmt == 'l') { lng=1; fmt++; if (*fmt=='l') { lng=2; fmt++; } }
        else if (*fmt == 'h') { fmt++; if (*fmt=='h') { fmt++; lng=-1; } }
        else if (*fmt == 'z') { lng=3; fmt++; }

        char spec = *fmt++;

        switch (spec) {
        case 'c': {
            char c = (char)va_arg(ap, int);
            PUT(c); break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            size_t slen = 0;
            while (s[slen] && (prec<0||(int)slen<prec)) slen++;
            int pad = (width > (int)slen) ? width - (int)slen : 0;
            if (!left) while (pad-- > 0) PUT(' ');
            for (size_t i=0; i<slen; i++) PUT(s[i]);
            if ( left) while (pad-- > 0) PUT(' ');
            break;
        }
        case 'd': case 'i': {
            long long v;
            if (lng==2)       v=va_arg(ap,long long);
            else if (lng==1)  v=va_arg(ap,long);
            else if (lng==-1) v=(signed char)va_arg(ap,int);
            else              v=va_arg(ap,int);
            int neg = v<0;
            unsigned long long uv = neg ? -(unsigned long long)v : (unsigned long long)v;
            char tmp[64]; int tlen = itoa_buf(tmp, sizeof(tmp), uv, 10, 0, width, zero_pad, 1, neg);
            for (int i=0; i<tlen; i++) PUT(tmp[i]);
            break;
        }
        case 'u': {
            unsigned long long v;
            if (lng==2)      v=va_arg(ap,unsigned long long);
            else if (lng==1) v=va_arg(ap,unsigned long);
            else if (lng==3) v=va_arg(ap,size_t);
            else             v=va_arg(ap,unsigned int);
            char tmp[64]; int tlen = itoa_buf(tmp, sizeof(tmp), v, 10, 0, width, zero_pad, 0, 0);
            for (int i=0; i<tlen; i++) PUT(tmp[i]);
            break;
        }
        case 'x': case 'X': {
            unsigned long long v;
            if (lng==2)      v=va_arg(ap,unsigned long long);
            else if (lng==1) v=va_arg(ap,unsigned long);
            else             v=va_arg(ap,unsigned int);
            if (alt && v) { PUT('0'); PUT(spec=='X'?'X':'x'); }
            char tmp[64]; int tlen = itoa_buf(tmp, sizeof(tmp), v, 16, spec=='X', width, zero_pad, 0, 0);
            for (int i=0; i<tlen; i++) PUT(tmp[i]);
            break;
        }
        case 'o': {
            unsigned long long v = va_arg(ap, unsigned long);
            char tmp[64]; int tlen = itoa_buf(tmp, sizeof(tmp), v, 8, 0, width, zero_pad, 0, 0);
            for (int i=0; i<tlen; i++) PUT(tmp[i]);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            PUT('0'); PUT('x');
            char tmp[64]; int tlen = itoa_buf(tmp, sizeof(tmp), v, 16, 0, 0, 1, 0, 0);
            for (int i=0; i<tlen; i++) PUT(tmp[i]);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
            /* basic float support */
            double v = va_arg(ap, double);
            int p2 = (prec < 0) ? 6 : prec;
            int neg = v < 0;
            if (neg) v = -v;
            long long ipart = (long long)v;
            double fpart = v - (double)ipart;
            char tmp[64]; int tlen = itoa_buf(tmp, sizeof(tmp), (unsigned long long)ipart, 10, 0, 0, 0, 0, 0);
            if (neg) PUT('-');
            for (int i=0; i<tlen; i++) PUT(tmp[i]);
            PUT('.');
            for (int i=0; i<p2; i++) {
                fpart *= 10;
                PUT('0' + (int)fpart % 10);
            }
            break;
        }
        case 'n':
            *va_arg(ap, int *) = n;
            break;
        case '%':
            PUT('%'); break;
        default:
            PUT('%'); PUT(spec); break;
        }
    }
    if (size > 0) buf[n < (int)size ? n : (int)size-1] = '\0';
    return n;
#undef PUT
}

int vero_snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vero_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int vero_sprintf(char *buf, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vero_vsnprintf(buf, 65536, fmt, ap);
    va_end(ap);
    return r;
}

int vero_printf(const char *fmt, ...) {
    char buf[4096];
    va_list ap; va_start(ap, fmt);
    int n = vero_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    vero_write(1, buf, n < (int)sizeof(buf) ? n : (int)sizeof(buf)-1);
    return n;
}

int vero_fprintf_fd(int fd, const char *fmt, ...) {
    char buf[4096];
    va_list ap; va_start(ap, fmt);
    int n = vero_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    vero_write(fd, buf, n < (int)sizeof(buf) ? n : (int)sizeof(buf)-1);
    return n;
}

int vero_puts(const char *s) {
    size_t n = vero_strlen(s);
    vero_write(1, s, n);
    vero_write(1, "\n", 1);
    return (int)n + 1;
}

void vero_putchar(int c) {
    char ch = (char)c;
    vero_write(1, &ch, 1);
}

int vero_getchar(void) {
    char c;
    long r = vero_read(0, &c, 1);
    return r <= 0 ? -1 : (unsigned char)c;
}

/* ── vero_itoa / vero_atoi ───────────────────────────────────── */
int vero_atoi(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int neg = 0;
    if (*s == '-') { neg=1; s++; }
    else if (*s == '+') s++;
    long long v = 0;
    while (*s >= '0' && *s <= '9') v = v*10 + (*s++ - '0');
    return (int)(neg ? -v : v);
}

long vero_atol(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int neg = 0;
    if (*s == '-') { neg=1; s++; }
    long long v = 0;
    while (*s >= '0' && *s <= '9') v = v*10 + (*s++ - '0');
    return neg ? -v : v;
}
