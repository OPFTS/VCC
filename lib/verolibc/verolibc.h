#ifndef VEROLIBC_H
#define VEROLIBC_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── I/O ──────────────────────────────────────────────────────── */
int    vero_printf(const char *fmt, ...);
int    vero_fprintf_fd(int fd, const char *fmt, ...);
int    vero_snprintf(char *buf, size_t size, const char *fmt, ...);
int    vero_sprintf(char *buf, const char *fmt, ...);
int    vero_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int    vero_puts(const char *s);
void   vero_putchar(int c);
int    vero_getchar(void);

/* ── Memory ───────────────────────────────────────────────────── */
void  *vero_malloc(size_t sz);
void  *vero_calloc(size_t n, size_t sz);
void  *vero_realloc(void *p, size_t sz);
void   vero_free(void *p);
void  *vero_memcpy(void *dst, const void *src, size_t n);
void  *vero_memmove(void *dst, const void *src, size_t n);
void  *vero_memset(void *s, int c, size_t n);
int    vero_memcmp(const void *a, const void *b, size_t n);
void  *vero_memchr(const void *s, int c, size_t n);

/* ── String ───────────────────────────────────────────────────── */
size_t vero_strlen(const char *s);
size_t vero_strnlen(const char *s, size_t maxlen);
char  *vero_strcpy(char *dst, const char *src);
char  *vero_strncpy(char *dst, const char *src, size_t n);
size_t vero_strlcpy(char *dst, const char *src, size_t sz);
char  *vero_strcat(char *dst, const char *src);
char  *vero_strncat(char *dst, const char *src, size_t n);
size_t vero_strlcat(char *dst, const char *src, size_t sz);
int    vero_strcmp(const char *a, const char *b);
int    vero_strncmp(const char *a, const char *b, size_t n);
int    vero_strcasecmp(const char *a, const char *b);
char  *vero_strchr(const char *s, int c);
char  *vero_strrchr(const char *s, int c);
char  *vero_strstr(const char *haystack, const char *needle);
char  *vero_strtok_r(char *str, const char *delim, char **save);

/* ── Conversion ───────────────────────────────────────────────── */
int    vero_atoi(const char *s);
long   vero_atol(const char *s);

/* ── Math ─────────────────────────────────────────────────────── */
double vero_sqrt(double x);
double vero_pow(double x, double y);
double vero_exp(double x);
double vero_log(double x);
double vero_sin(double x);
double vero_cos(double x);
double vero_tan(double x);
double vero_floor(double x);
double vero_ceil(double x);
double vero_round(double x);
double vero_trunc(double x);
double vero_fmod(double x, double y);

/* ── stdlib ───────────────────────────────────────────────────── */
void   vero_exit(int code) __attribute__((noreturn));
void   vero_abort(void)    __attribute__((noreturn));
long   vero_getpid(void);
void   vero_qsort(void *base, size_t n, size_t sz,
                  int (*cmp)(const void *, const void *));
void  *vero_bsearch(const void *key, const void *base,
                    size_t n, size_t sz,
                    int (*cmp)(const void *, const void *));
int    vero_abs(int x);
long   vero_labs(long x);
int    vero_rand(void);
void   vero_srand(unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif /* VEROLIBC_H */
