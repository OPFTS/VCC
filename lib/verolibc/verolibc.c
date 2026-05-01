#include "verolibc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

int   vcc_printf(const char *fmt,...){va_list a;va_start(a,fmt);int r=vprintf(fmt,a);va_end(a);return r;}
int   vcc_scanf(const char *fmt,...) {va_list a;va_start(a,fmt);int r=vscanf(fmt,a);va_end(a);return r;}
int   vcc_puts(const char *s)        {return puts(s);}
char *vcc_gets(char *buf,int n)      {return fgets(buf,n,stdin);}
void  vcc_putchar(int c)             {putchar(c);}
int   vcc_getchar(void)              {return getchar();}
void *vcc_malloc(size_t sz)          {return malloc(sz);}
void *vcc_calloc(size_t n,size_t sz) {return calloc(n,sz);}
void *vcc_realloc(void *p,size_t sz) {return realloc(p,sz);}
void  vcc_free(void *p)              {free(p);}
void *vcc_memcpy(void *d,const void *s,size_t n){return memcpy(d,s,n);}
void *vcc_memset(void *s,int c,size_t n){return memset(s,c,n);}
size_t vcc_strlen(const char *s)    {return strlen(s);}
char  *vcc_strcpy(char *d,const char *s){return strcpy(d,s);}
char  *vcc_strncpy(char *d,const char *s,size_t n){return strncpy(d,s,n);}
int    vcc_strcmp(const char *a,const char *b){return strcmp(a,b);}
int    vcc_strncmp(const char *a,const char *b,size_t n){return strncmp(a,b,n);}
char  *vcc_strcat(char *d,const char *s){return strcat(d,s);}
char  *vcc_strchr(const char *s,int c){return (char*)strchr(s,c);}
double vcc_sqrt(double x)    {return sqrt(x);}
double vcc_pow(double x,double y){return pow(x,y);}
double vcc_sin(double x)     {return sin(x);}
double vcc_cos(double x)     {return cos(x);}
int    vcc_abs(int x)        {return abs(x);}
void   vcc_exit(int c)       {exit(c);}
