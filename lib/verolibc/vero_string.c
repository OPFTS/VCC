/*
 * vero_string.c — VeroCC string library
 * Pure implementation, no libc dependency.
 */
#include <stddef.h>
#include <stdint.h>

/* ── vero_memset ─────────────────────────────────────────────── */
void *vero_memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    unsigned char  v = (unsigned char)c;
    /* word-aligned fast path */
    while (n && ((uintptr_t)p & 7)) { *p++ = v; n--; }
    uint64_t wide = (uint64_t)v * 0x0101010101010101ULL;
    uint64_t *wp = (uint64_t *)p;
    while (n >= 8) { *wp++ = wide; n -= 8; }
    p = (unsigned char *)wp;
    while (n--) *p++ = v;
    return s;
}

/* ── vero_memcpy ─────────────────────────────────────────────── */
void *vero_memcpy(void *dst, const void *src, size_t n) {
    char       *d = (char *)dst;
    const char *s = (const char *)src;
    /* word-aligned fast path (non-overlapping assumed) */
    while (n && ((uintptr_t)d & 7)) { *d++ = *s++; n--; }
    uint64_t *dw = (uint64_t *)d;
    const uint64_t *sw = (const uint64_t *)s;
    while (n >= 8) { *dw++ = *sw++; n -= 8; }
    d = (char *)dw; s = (const char *)sw;
    while (n--) *d++ = *s++;
    return dst;
}

void *vero_memmove(void *dst, const void *src, size_t n) {
    char       *d = (char *)dst;
    const char *s = (const char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int vero_memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

void *vero_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }
    return NULL;
}

/* ── vero_strlen ─────────────────────────────────────────────── */
size_t vero_strlen(const char *s) {
    const char *p = s;
    /* word-aligned scan */
    while ((uintptr_t)p & 7) { if (!*p) return p - s; p++; }
    const uint64_t *wp = (const uint64_t *)p;
    for (;;) {
        uint64_t w = *wp;
        /* check for any zero byte in word */
        if ((w - 0x0101010101010101ULL) & ~w & 0x8080808080808080ULL) {
            const char *cp = (const char *)wp;
            while (*cp) cp++;
            return cp - s;
        }
        wp++;
    }
}

size_t vero_strnlen(const char *s, size_t maxlen) {
    const char *p = s;
    while (maxlen-- && *p) p++;
    return p - s;
}

/* ── vero_strcpy / strncpy ───────────────────────────────────── */
char *vero_strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *vero_strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

/* ── vero_strcat ─────────────────────────────────────────────── */
char *vero_strcat(char *dst, const char *src) {
    char *d = dst + vero_strlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char *vero_strncat(char *dst, const char *src, size_t n) {
    char *d = dst + vero_strlen(dst);
    while (n-- && *src) *d++ = *src++;
    *d = '\0';
    return dst;
}

/* Safer: always NUL-terminates, returns total length */
size_t vero_strlcpy(char *dst, const char *src, size_t sz) {
    size_t srclen = vero_strlen(src);
    if (sz > 0) {
        size_t copy = srclen < sz-1 ? srclen : sz-1;
        vero_memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return srclen;
}

size_t vero_strlcat(char *dst, const char *src, size_t sz) {
    size_t dstlen = vero_strnlen(dst, sz);
    if (dstlen == sz) return sz + vero_strlen(src);
    return dstlen + vero_strlcpy(dst+dstlen, src, sz-dstlen);
}

/* ── vero_strcmp ─────────────────────────────────────────────── */
int vero_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int vero_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (!n) return 0;  /* n reached 0 after pre-decrement... */
    return (unsigned char)*a - (unsigned char)*b;
}

int vero_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a+32 : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b+32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ── vero_strchr / strrchr ───────────────────────────────────── */
char *vero_strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char *)s; s++; }
    return (c == '\0') ? (char *)s : NULL;
}

char *vero_strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) { if (*s == (char)c) last = s; s++; }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

/* ── vero_strstr ─────────────────────────────────────────────── */
char *vero_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    size_t nlen = vero_strlen(needle);
    while (*haystack) {
        if (*haystack == *needle &&
            !vero_memcmp(haystack, needle, nlen))
            return (char *)haystack;
        haystack++;
    }
    return NULL;
}

/* ── vero_strtok_r ───────────────────────────────────────────── */
char *vero_strtok_r(char *str, const char *delim, char **save) {
    if (!str) str = *save;
    /* skip leading delimiters */
    while (*str && vero_strchr(delim, *str)) str++;
    if (!*str) { *save = str; return NULL; }
    char *tok = str;
    while (*str && !vero_strchr(delim, *str)) str++;
    if (*str) { *str++ = '\0'; }
    *save = str;
    return tok;
}
