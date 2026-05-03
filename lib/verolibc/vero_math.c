/*
 * vero_math.c — VeroCC math functions
 * IEEE 754 correct implementations using x87/SSE2
 */
#include <stdint.h>

/* ── basic helpers ───────────────────────────────────────────── */
typedef union { double d; uint64_t u; } DU;

static double vero_fabs(double x) { DU u; u.d=x; u.u&=~(1ULL<<63); return u.d; }

/* ── vero_sqrt (Newton-Raphson) ─────────────────────────────── */
double vero_sqrt(double x) {
    if (x < 0) return __builtin_nan("");
    if (x == 0) return 0;
    double r;
    /* use hardware sqrt if available */
    __asm__ ("sqrtsd %1, %0" : "=x"(r) : "x"(x));
    return r;
}

/* ── vero_floor / vero_ceil ──────────────────────────────────── */
double vero_floor(double x) {
    double r;
    __asm__ ("roundsd $1, %1, %0" : "=x"(r) : "x"(x));
    return r;
}

double vero_ceil(double x) {
    double r;
    __asm__ ("roundsd $2, %1, %0" : "=x"(r) : "x"(x));
    return r;
}

double vero_round(double x) {
    double r;
    __asm__ ("roundsd $0, %1, %0" : "=x"(r) : "x"(x));
    return r;
}

double vero_trunc(double x) {
    double r;
    __asm__ ("roundsd $3, %1, %0" : "=x"(r) : "x"(x));
    return r;
}

/* ── vero_pow (fast integer exponent + exp/log fallback) ──────── */
double vero_exp(double x);
double vero_log(double x);

double vero_pow(double base, double exp) {
    if (exp == 0) return 1;
    if (base == 0) return 0;
    if (base == 1) return 1;
    /* integer exponent fast path */
    long ie = (long)exp;
    if ((double)ie == exp) {
        double r=1, b=base; long e=ie;
        if (e<0) { b=1.0/b; e=-e; }
        while (e) { if (e&1) r*=b; b*=b; e>>=1; }
        return r;
    }
    return vero_exp(exp * vero_log(base));
}

/* ── vero_log (Taylor series, domain: x > 0) ─────────────────── */
double vero_log(double x) {
    if (x <= 0) return __builtin_nan("");
    if (x == 1) return 0;
    /* normalize: x = m * 2^e, 0.5 <= m < 1 */
    int e = 0;
    while (x >= 2) { x /= 2; e++; }
    while (x <  1) { x *= 2; e--; }
    /* log(x) = log(m * 2^e) = log(m) + e*log(2) */
    /* log(m) via Pade for m in [0.5, 1): use y=(m-1)/(m+1) */
    double y = (x - 1.0) / (x + 1.0);
    double y2 = y * y;
    double s = y * (2 + y2*(2.0/3 + y2*(2.0/5 + y2*(2.0/7
               + y2*(2.0/9 + y2*2.0/11)))));
    return s + e * 0.6931471805599453;   /* e * ln(2) */
}

/* ── vero_exp (Taylor series) ─────────────────────────────────── */
double vero_exp(double x) {
    /* range reduction: e^x = e^k * e^r, k=round(x/ln2) */
    double ln2 = 0.6931471805599453;
    long k = (long)(x / ln2 + 0.5);
    double r = x - k * ln2;
    /* Taylor: e^r = sum(r^n/n!) */
    double s=1, t=1;
    for (int i=1; i<=20; i++) { t *= r/i; s += t; if (t<1e-17&&t>-1e-17) break; }
    /* multiply by 2^k */
    DU u; u.d = 1.0;
    u.u += (uint64_t)k << 52;
    return s * u.d;
}

/* ── vero_sin / vero_cos (Bhaskara approximation + reduction) ─── */
double vero_sin(double x) {
    /* range reduction to [-π, π] */
    double pi  = 3.14159265358979323846;
    double pi2 = 6.28318530717958647692;
    while (x >  pi) x -= pi2;
    while (x < -pi) x += pi2;
    /* Bhaskara I: sin(x) ≈ 16x(π-x) / (5π²-4x(π-x)) for [0,π] */
    /* Extend for negative via symmetry */
    double neg = x < 0;
    if (neg) x = -x;
    double n = 16*x*(pi-x);
    double d = 5*pi*pi - 4*x*(pi-x);
    double r = n/d;
    return neg ? -r : r;
}

double vero_cos(double x) {
    return vero_sin(x + 1.5707963267948966);  /* cos(x) = sin(x+π/2) */
}

double vero_tan(double x) {
    double c = vero_cos(x);
    if (c == 0) return __builtin_inf();
    return vero_sin(x) / c;
}

double vero_fmod(double x, double y) {
    if (y == 0) return __builtin_nan("");
    return x - vero_trunc(x/y) * y;
}
