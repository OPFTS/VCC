#ifndef VEROLIBC_H
#define VEROLIBC_H

/* VeroCC standard C library wrapper */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* I/O */
int   vcc_printf(const char *fmt, ...);
int   vcc_scanf(const char *fmt, ...);
int   vcc_puts(const char *s);
char *vcc_gets(char *buf, int n);
void  vcc_putchar(int c);
int   vcc_getchar(void);

/* Memory */
void *vcc_malloc(size_t sz);
void *vcc_calloc(size_t n, size_t sz);
void *vcc_realloc(void *p, size_t sz);
void  vcc_free(void *p);
void *vcc_memcpy(void *dst, const void *src, size_t n);
void *vcc_memset(void *s, int c, size_t n);

/* String */
size_t vcc_strlen(const char *s);
char  *vcc_strcpy(char *dst, const char *src);
char  *vcc_strncpy(char *dst, const char *src, size_t n);
int    vcc_strcmp(const char *a, const char *b);
int    vcc_strncmp(const char *a, const char *b, size_t n);
char  *vcc_strcat(char *dst, const char *src);
char  *vcc_strchr(const char *s, int c);

/* Math */
double vcc_sqrt(double x);
double vcc_pow(double x, double y);
double vcc_sin(double x);
double vcc_cos(double x);
int    vcc_abs(int x);

/* Process */
void   vcc_exit(int code);

#endif /* VEROLIBC_H */
