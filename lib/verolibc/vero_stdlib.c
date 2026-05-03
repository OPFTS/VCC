/*
 * vero_stdlib.c — VeroCC stdlib functions
 * process control, environment, conversion
 */
#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/syscall.h"
#include "verolibc.h"

void vero_exit(int code) {
    __vero_exit_raw(code);
    __builtin_unreachable();
}

void vero_abort(void) {
    /* send SIGABRT to self */
    __syscall2(SYS_kill, __vero_getpid_raw(), 6);
    __syscall1(SYS_exit_group, 134);
    __builtin_unreachable();
}

long vero_getpid(void) {
    return __vero_getpid_raw();
}

/* ── vero_qsort (introsort base: simple qsort) ─────────────── */
static void swap_bytes(char *a, char *b, size_t sz) {
    char tmp;
    while (sz--) { tmp=*a; *a++=*b; *b++=tmp; }
}

static void insertion_sort(char *base, size_t n, size_t sz,
    int (*cmp)(const void *, const void *)) {
    for (size_t i=1; i<n; i++) {
        for (size_t j=i; j>0 && cmp(base+(j-1)*sz, base+j*sz)>0; j--)
            swap_bytes(base+(j-1)*sz, base+j*sz, sz);
    }
}

static void _qsort(char *lo, char *hi, size_t sz,
    int (*cmp)(const void *, const void *)) {
    if (hi - lo < (long)(16 * sz)) {
        insertion_sort(lo, (hi-lo)/sz + 1, sz, cmp);
        return;
    }
    char *pivot = lo + ((hi-lo)/sz/2)*sz;
    swap_bytes(pivot, hi, sz);
    char *store = lo;
    for (char *p=lo; p<hi; p+=sz)
        if (cmp(p, hi) < 0) { swap_bytes(p, store, sz); store+=sz; }
    swap_bytes(store, hi, sz);
    if (store > lo+sz) _qsort(lo, store-sz, sz, cmp);
    if (store < hi-sz) _qsort(store+sz, hi, sz, cmp);
}

void vero_qsort(void *base, size_t n, size_t sz,
                int (*cmp)(const void *, const void *)) {
    if (n < 2 || sz == 0) return;
    _qsort((char *)base, (char *)base + (n-1)*sz, sz, cmp);
}

/* ── vero_bsearch ────────────────────────────────────────────── */
void *vero_bsearch(const void *key, const void *base,
                   size_t n, size_t sz,
                   int (*cmp)(const void *, const void *)) {
    const char *lo = (const char *)base;
    const char *hi = lo + n * sz;
    while (lo < hi) {
        const char *mid = lo + (((hi-lo)/sz)/2)*sz;
        int r = cmp(key, mid);
        if (r == 0) return (void *)mid;
        else if (r < 0) hi = mid;
        else lo = mid + sz;
    }
    return NULL;
}

/* ── vero_abs / vero_labs ─────────────────────────────────────── */
int  vero_abs (int  x) { return x < 0 ? -x : x; }
long vero_labs(long x) { return x < 0 ? -x : x; }

/* ── simple PRNG (xorshift64) ─────────────────────────────────── */
static unsigned long long _rand_state = 0x123456789ABCDEFULL;
int vero_rand(void) {
    _rand_state ^= _rand_state << 13;
    _rand_state ^= _rand_state >> 7;
    _rand_state ^= _rand_state << 17;
    return (int)(_rand_state & 0x7FFFFFFF);
}
void vero_srand(unsigned int seed) {
    _rand_state = seed ? (unsigned long long)seed : 1;
}
