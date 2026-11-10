/* C23 <math.h> functions that msvcrt.dll lacks (part of libtcc1.a on
   Windows targets), in addition to those in c23rt.c:

   acospi asinpi atanpi atan2pi cospi sinpi tanpi
   exp10 exp10m1 exp2m1 log10p1 log2p1 logp1
   compoundn pown powr rootn rsqrt
   fromfp ufromfp fromfpx ufromfpx canonicalize
   (each also with f and l suffix), and __issignaling{,f,l} for the
   issignaling macro.

   The float versions compute in double and round once at the end.
   long double is double on these targets, so the l variants call the
   double ones.  Built on msvcrt's sin, cos, tan, asin, acos, atan, atan2,
   exp, log, pow, sqrt, floor and ceil; results that C23 requires to be
   exact (integers and half-integers for the pi functions, exact powers
   and roots, special values) are handled here.  Floating-point exception
   flags and errno are not set.  Uses no Windows API and can be tested
   natively. */

#include <stdint.h>

void *memcpy(void *, const void *, __SIZE_TYPE__);

double sin(double);
double cos(double);
double tan(double);
double asin(double);
double acos(double);
double atan(double);
double atan2(double, double);
double exp(double);
double log(double);
double pow(double, double);
double sqrt(double);
double floor(double);
double ceil(double);
double roundeven(double);   /* c23rt.c */

#define PI    3.14159265358979323846
#define LN2   0.693147180559945309417
#define LN10  2.30258509299404568402

/* FP_INT_* of <math.h> */
#define FP_INT_UPWARD 0
#define FP_INT_DOWNWARD 1
#define FP_INT_TOWARDZERO 2
#define FP_INT_TONEARESTFROMZERO 3
#define FP_INT_TONEAREST 4

static uint64_t d2u(double x) { uint64_t u; memcpy(&u, &x, 8); return u; }
static double u2d(uint64_t u) { double x; memcpy(&x, &u, 8); return x; }
static uint32_t f2u(float x) { uint32_t u; memcpy(&u, &x, 4); return u; }
static float u2f(uint32_t u) { float x; memcpy(&x, &u, 4); return x; }

static int d_isnan(double x) { return (d2u(x) << 1) > 0xffe0000000000000ull; }
static int d_isinf(double x) { return (d2u(x) << 1) == 0xffe0000000000000ull; }
static int d_neg(double x) { return (int)(d2u(x) >> 63); }
static double d_abs(double x) { return u2d(d2u(x) & 0x7fffffffffffffffull); }
static double d_copysign(double x, double s)
{
    return u2d((d2u(x) & 0x7fffffffffffffffull) | (d2u(s) & 0x8000000000000000ull));
}
static double d_nan(void) { return u2d(0x7ff8000000000000ull); }
static double d_inf(void) { return u2d(0x7ff0000000000000ull); }
/* integral (inf excluded) */
static int d_isint(double x) { return !d_isnan(x) && !d_isinf(x) && floor(x) == x; }
/* 2^n, -1022 <= n <= 1023 */
static double d_pow2(int n) { return u2d((uint64_t)(n + 1023) << 52); }
/* 10^n, 0 <= n <= 22: exact */
static double d_pow10(int n)
{
    double p = 1.0;
    while (n-- > 0)
        p *= 10.0;
    return p;
}
static double d_trunc(double x) { return d_neg(x) ? ceil(x) : floor(x); }
static double d_round(double x)
{
    double t = d_trunc(x);
    if (d_abs(x - t) >= 0.5)
        t += d_neg(x) ? -1.0 : 1.0;
    return t;
}

/* exp(x) - 1 for |x| < 3 (Kahan) */
static double em1(double x)
{
    double u = exp(x), um1;
    if (u == 1.0)
        return x;
    um1 = u - 1.0;
    if (um1 == -1.0)
        return -1.0;
    return um1 * x / log(u);
}

/* log(1 + x) for finite x > -1 */
static double lp1(double x)
{
    double u = 1.0 + x;
    if (u == 1.0)
        return x;
    return log(u) * (x / (u - 1.0));
}

/* ------------------------------------------------------------------ */
/* pi functions */

double sinpi(double x)
{
    double a = d_abs(x), y, r;
    int neg = 0;

    if (d_isnan(x))
        return x + x;
    if (d_isinf(x))
        return d_nan();
    if (a >= 4503599627370496.0)    /* 2^52: integral */
        return d_copysign(0.0, x);
    y = a - 2.0 * floor(a * 0.5);   /* [0, 2), exact */
    if (y >= 1.0) {
        y -= 1.0;
        neg = 1;
    }
    if (y > 0.5)
        y = 1.0 - y;                /* [0, 0.5] */
    if (y == 0.0)
        return d_copysign(0.0, x);
    if (y == 0.5)
        r = 1.0;
    else if (y <= 0.25)
        r = sin(PI * y);
    else
        r = cos(PI * (0.5 - y));
    if (neg)
        r = -r;
    return d_neg(x) ? -r : r;
}

double cospi(double x)
{
    double a = d_abs(x), y, r;
    int neg = 0;

    if (d_isnan(x))
        return x + x;
    if (d_isinf(x))
        return d_nan();
    if (a >= 9007199254740992.0)    /* 2^53: even */
        return 1.0;
    y = a - 2.0 * floor(a * 0.5);   /* [0, 2) */
    if (y > 1.0)
        y = 2.0 - y;                /* [0, 1] */
    if (y > 0.5) {
        y = 1.0 - y;                /* [0, 0.5) */
        neg = 1;
    }
    if (y == 0.5)
        return 0.0;                 /* +0 at half-integers */
    if (y == 0.0)
        r = 1.0;
    else if (y <= 0.25)
        r = cos(PI * y);
    else
        r = sin(PI * (0.5 - y));
    return neg ? -r : r;
}

double tanpi(double x)
{
    double a = d_abs(x), y, r;
    int odd = 0;

    if (d_isnan(x))
        return x + x;
    if (d_isinf(x))
        return d_nan();
    if (a >= 9007199254740992.0) {  /* even integer */
        y = 0.0;
    } else {
        y = a - 2.0 * floor(a * 0.5);
        if (y >= 1.0) {
            y -= 1.0;
            odd = 1;
        }
    }
    /* tanpi(a) for y in [0, 1) */
    if (y == 0.0)
        r = odd ? u2d(0x8000000000000000ull) : 0.0;
    else if (y == 0.5)
        r = odd ? -d_inf() : d_inf();
    else {
        int neg = 0;
        if (y > 0.5) {
            y = 1.0 - y;
            neg = 1;
        }
        if (y == 0.25)
            r = 1.0;
        else if (y < 0.25)
            r = tan(PI * y);
        else
            r = 1.0 / tan(PI * (0.5 - y));
        if (neg)
            r = -r;
    }
    return d_neg(x) ? -r : r;
}

double asinpi(double x)
{
    if (d_isnan(x))
        return x + x;
    if (d_abs(x) > 1.0)
        return d_nan();
    if (d_abs(x) == 1.0)
        return d_copysign(0.5, x);
    if (x == 0.0)
        return x;
    return asin(x) / PI;
}

double acospi(double x)
{
    if (d_isnan(x))
        return x + x;
    if (d_abs(x) > 1.0)
        return d_nan();
    if (x == 1.0)
        return 0.0;
    if (x == -1.0)
        return 1.0;
    if (x == 0.0)
        return 0.5;
    return acos(x) / PI;
}

double atanpi(double x)
{
    if (d_isnan(x))
        return x + x;
    if (d_isinf(x))
        return d_copysign(0.5, x);
    if (x == 0.0)
        return x;
    if (d_abs(x) == 1.0)
        return d_copysign(0.25, x);
    return atan(x) / PI;
}

double atan2pi(double y, double x)
{
    if (d_isnan(x) || d_isnan(y))
        return x + y;
    if (y == 0.0)
        return d_copysign(d_neg(x) ? 1.0 : 0.0, y);
    if (d_isinf(x)) {
        if (d_isinf(y))
            return d_copysign(d_neg(x) ? 0.75 : 0.25, y);
        return d_copysign(d_neg(x) ? 1.0 : 0.0, y);
    }
    if (x == 0.0 || d_isinf(y))
        return d_copysign(0.5, y);
    if (d_abs(x) == d_abs(y))
        return d_copysign(d_neg(x) ? 0.75 : 0.25, y);
    return atan2(y, x) / PI;
}

/* ------------------------------------------------------------------ */
/* exponentials and logarithms */

double exp10(double x)
{
    double p;
    if (d_isnan(x))
        return x + x;
    if (d_isinf(x))
        return d_neg(x) ? 0.0 : x;
    if (d_isint(x) && d_abs(x) <= 22.0) {
        p = d_pow10((int)d_abs(x));
        return d_neg(x) ? 1.0 / p : p;
    }
    if (x > 400.0)
        return d_inf();
    if (x < -400.0)
        return 0.0;
    return pow(10.0, x);
}

double exp10m1(double x)
{
    double p;
    if (d_isnan(x) || x == 0.0)
        return x + x;
    if (d_isinf(x))
        return d_neg(x) ? -1.0 : x;
    if (d_isint(x) && d_abs(x) <= 22.0) {
        p = d_pow10((int)d_abs(x));
        return d_neg(x) ? (1.0 - p) / p : p - 1.0;
    }
    if (d_abs(x) < 0.3)
        return em1(x * LN10);
    if (x < -20.0)
        return -1.0;
    if (x > 400.0)
        return d_inf();
    return pow(10.0, x) - 1.0;
}

double exp2m1(double x)
{
    if (d_isnan(x) || x == 0.0)
        return x + x;
    if (d_isinf(x))
        return d_neg(x) ? -1.0 : x;
    if (x < -60.0)
        return -1.0;
    if (x > 1100.0)
        return d_inf();
    if (d_isint(x) && x <= 1023.0)
        return d_pow2((int)x) - 1.0;
    if (d_abs(x) < 1.0)
        return em1(x * LN2);
    return pow(2.0, x) - 1.0;
}

/* NaN, -1, < -1, +inf; else returns 0 and the caller computes */
static int logp1_special(double x, double *r)
{
    if (d_isnan(x))
        *r = x + x;
    else if (x == -1.0)
        *r = -d_inf();
    else if (x < -1.0)
        *r = d_nan();
    else if (d_isinf(x))
        *r = x;
    else
        return 0;
    return 1;
}

double logp1(double x)
{
    double r;
    if (logp1_special(x, &r))
        return r;
    return lp1(x);
}

double log2p1(double x)
{
    double r, u;
    uint64_t b;
    if (logp1_special(x, &r))
        return r;
    if (x == 0.0)
        return x;
    u = 1.0 + x;
    b = d2u(u);
    if (u - 1.0 == x && (b & 0x000fffffffffffffull) == 0 && (b >> 52) != 0)
        return (double)((int)(b >> 52) - 1023);     /* exact power of 2 */
    return lp1(x) / LN2;
}

double log10p1(double x)
{
    double r, u;
    int n;
    if (logp1_special(x, &r))
        return r;
    if (x == 0.0)
        return x;
    u = 1.0 + x;
    if (u - 1.0 == x && u >= 1.0 && u <= 1e22) {
        for (n = 0; n <= 22; n++)
            if (d_pow10(n) == u)
                return (double)n;                   /* exact power of 10 */
    }
    return lp1(x) / LN10;
}

/* ------------------------------------------------------------------ */
/* powers and roots */

static int ll_odd(long long n) { return (int)(n & 1); }

/* double-double arithmetic (Dekker), for exact and accurate powers;
   needs operands in about [2^-969, 2^995] */
static void two_prod(double a, double b, double *p, double *e)
{
    double c, ah, al, bh, bl;
    *p = a * b;
    c = 134217729.0 * a;                /* 2^27 + 1 */
    ah = c - (c - a);
    al = a - ah;
    c = 134217729.0 * b;
    bh = c - (c - b);
    bl = b - bh;
    *e = ((ah * bh - *p) + ah * bl + al * bh) + al * bl;
}

static void dd_mul(double *h, double *l, double bh, double bl)
{
    double p, e, s;
    two_prod(*h, bh, &p, &e);
    e += *h * bl + *l * bh;
    s = p + e;
    *l = e - (s - p);
    *h = s;
}

static int dd_ok(double h)
{
    double a = d_abs(h);
    return a > 0x1p-969 && a < 0x1p995;
}

/* (bh + bl)^m by squaring, m >= 1, into *h + *l; 0 if out of range */
static int dd_pow(double bh, double bl, unsigned long long m, double *h, double *l)
{
    int first = 1;
    *h = 1.0, *l = 0.0;
    while (1) {
        if (!dd_ok(bh))
            return 0;
        if (m & 1) {
            if (first)
                *h = bh, *l = bl, first = 0;
            else
                dd_mul(h, l, bh, bl);
            if (!dd_ok(*h))
                return 0;
        }
        m >>= 1;
        if (!m)
            return 1;
        dd_mul(&bh, &bl, bh, bl);
    }
}

/* (bh + bl)^n for n != 0, or 0 with *r unset if out of range */
static int dd_pown(double bh, double bl, long long n, double *r)
{
    unsigned long long m = n < 0 ? 0ull - (unsigned long long)n : (unsigned long long)n;
    double h, l, q, p, e;
    if (!dd_pow(bh, bl, m, &h, &l))
        return 0;
    if (n > 0) {
        *r = h + l;
        return 1;
    }
    q = 1.0 / h;                        /* 1/(h + l), refined once */
    two_prod(h, q, &p, &e);
    *r = q + q * (((1.0 - p) - e) - l * q);
    return 1;
}

double pown(double x, long long n)
{
    double r;

    if (n == 0)
        return 1.0;
    if (d_isnan(x))
        return x + x;
    if (x == 0.0) {
        if (n > 0)
            return ll_odd(n) ? x : 0.0;
        return ll_odd(n) ? d_copysign(d_inf(), x) : d_inf();
    }
    if (d_isinf(x)) {
        if (n > 0)
            return ll_odd(n) ? x : d_inf();
        return ll_odd(n) ? d_copysign(0.0, x) : 0.0;
    }
    {
        /* x = +-2^e exactly: the result is +-2^(n*e), built from its bits
           with round-to-nearest-even, since msvcrt's pow() is not exact
           for subnormal results (e.g. pown(2, -1075) must be 0) */
        uint64_t u = d2u(x);
        int be = (int)(u >> 52 & 0x7ff);
        if ((u & 0xfffffffffffffull) == 0 && be != 0) {
            long long k, e = be - 1023;
            uint64_t sign = ll_odd(n) ? (u & 0x8000000000000000ull) : 0;
            if (e == 0)
                return u2d(sign | 0x3ff0000000000000ull); /* +-1 */
            /* n*e without overflow: |n*e| > 2000 is out of range anyway */
            k = (n > 4000 || n < -4000) ? (((n > 0) == (e > 0)) ? 4000 : -4000)
                                        : n * e;
            if (k > 1023)
                return u2d(sign | 0x7ff0000000000000ull);       /* inf */
            if (k >= -1022)
                return u2d(sign | (uint64_t)(k + 1023) << 52);
            if (k >= -1074)
                return u2d(sign | 1ull << (k + 1074));         /* subnormal */
            return u2d(sign);              /* 2^-1075 ties to even: 0 */
        }
    }
    if (dd_pown(x, 0.0, n, &r))
        return r;
    return pow(x, (double)n);           /* near overflow or underflow */
}

double compoundn(double x, long long n)
{
    double u, r;
    if (n == 0)
        return 1.0;
    if (d_isnan(x))
        return x + x;
    if (x < -1.0)
        return d_nan();
    if (x == -1.0)
        return n < 0 ? d_inf() : 0.0;
    if (d_isinf(x))
        return n < 0 ? 0.0 : x;
    u = 1.0 + x;                        /* 1 + x = u + (lo) exactly */
    if (dd_pown(u, d_abs(x) <= 1.0 ? x - (u - 1.0) : 1.0 - (u - x), n, &r))
        return r;
    return exp((double)n * lp1(x));
}

double powr(double x, double y)
{
    if (d_isnan(x) || d_isnan(y))
        return x + y;
    if (x == 0.0) {
        if (y == 0.0)
            return d_nan();
        return d_neg(y) ? d_inf() : 0.0;
    }
    if (d_neg(x))
        return d_nan();
    if (d_isinf(x)) {
        if (y == 0.0)
            return d_nan();
        return d_neg(y) ? 0.0 : x;
    }
    if (x == 1.0)
        return d_isinf(y) ? d_nan() : 1.0;
    if (y == 0.0)
        return 1.0;
    if (d_isinf(y))
        return (x > 1.0) == !d_neg(y) ? d_inf() : 0.0;
    return pow(x, y);
}

double rootn(double x, long long n)
{
    unsigned long long m;
    double a, y, best, c, d, bd;
    int i;

    if (n == 0 || d_isnan(x))
        return n == 0 ? d_nan() : x + x;
    if (d_neg(x) && x != 0.0 && !ll_odd(n))
        return d_nan();
    if (x == 0.0) {
        if (n > 0)
            return ll_odd(n) ? x : 0.0;
        return ll_odd(n) ? d_copysign(d_inf(), x) : d_inf();
    }
    if (d_isinf(x)) {
        if (n > 0)
            return x;
        return d_copysign(0.0, x);
    }
    if (n == 1)
        return x;
    if (n == -1)
        return 1.0 / x;
    m = n < 0 ? 0ull - (unsigned long long)n : (unsigned long long)n;
    a = d_abs(x);
    y = pow(a, 1.0 / (double)m);    /* 1/m is inexact: up to ~230 ulps off */
    /* one Newton step: y += y * (a / y^m - 1) / m */
    c = pow(y, (double)m);
    if (c > 0.0 && !d_isinf(c))
        y += y * (a / c - 1.0) / (double)m;
    if (n < 0)
        y = 1.0 / y;
    /* pick the closest of the neighbours: z^n - a computed in double-double
       (exact roots such as rootn(-8, 3) come out exact) */
    if (dd_ok(a)) {
        best = y;
        bd = -1.0;
        for (i = -2; i <= 2; i++) {
            double h, l;
            c = u2d(d2u(y) + i);
            if (!dd_pow(c, 0.0, m, &h, &l))
                continue;
            if (n > 0) {
                d = d_abs((h - a) + l);
            } else {
                dd_mul(&h, &l, a, 0.0);
                d = d_abs((h - 1.0) + l);
            }
            if (bd < 0.0 || d < bd) {
                bd = d;
                best = c;
            }
        }
        y = best;
    }
    return d_neg(x) ? -y : y;
}

double rsqrt(double x)
{
    if (d_isnan(x))
        return x + x;
    if (x == 0.0)
        return d_copysign(d_inf(), x);
    if (x < 0.0)
        return d_nan();
    if (d_isinf(x))
        return 0.0;
    return 1.0 / sqrt(x);
}

/* ------------------------------------------------------------------ */
/* fromfp, canonicalize, issignaling */

static double rnd_int(double x, int rnd)
{
    switch (rnd) {
    case FP_INT_UPWARD:
        return ceil(x);
    case FP_INT_DOWNWARD:
        return floor(x);
    case FP_INT_TOWARDZERO:
        return d_trunc(x);
    case FP_INT_TONEARESTFROMZERO:
        return d_round(x);
    default:
        return roundeven(x);
    }
}

/* 2^w, inf for large w */
static double width_bound(unsigned w)
{
    return w > 1023 ? d_inf() : d_pow2((int)w);
}

double fromfp(double x, int rnd, unsigned width)
{
    double r, b;
    if (d_isnan(x) || d_isinf(x) || width == 0)
        return d_nan();
    r = rnd_int(x, rnd);
    b = width_bound(width - 1);
    return r >= -b && r < b ? r : d_nan();
}

double ufromfp(double x, int rnd, unsigned width)
{
    double r;
    if (d_isnan(x) || d_isinf(x) || width == 0)
        return d_nan();
    r = rnd_int(x, rnd);
    return r >= 0.0 && r < width_bound(width) ? r : d_nan();
}

/* the x variants differ only in raising "inexact", which is not tracked */
double fromfpx(double x, int rnd, unsigned width) { return fromfp(x, rnd, width); }
double ufromfpx(double x, int rnd, unsigned width) { return ufromfp(x, rnd, width); }

int canonicalize(double *cx, const double *x)
{
    uint64_t u = d2u(*x);
    if (d_isnan(*x))
        u |= 1ull << 51;                /* quiet a signaling NaN */
    *cx = u2d(u);
    return 0;
}

int canonicalizef(float *cx, const float *x)
{
    uint32_t u = f2u(*x);
    if ((u << 1) > 0xff000000u)
        u |= 1u << 22;
    *cx = u2f(u);
    return 0;
}

int canonicalizel(long double *cx, const long double *x)
{
    double d;
    int r = canonicalize(&d, (const double *)x);
    *cx = d;
    return r;
}

int __issignaling(double x)
{
    return d_isnan(x) && !(d2u(x) & (1ull << 51));
}

int __issignalingf(float x)
{
    uint32_t u = f2u(x);
    return (u << 1) > 0xff000000u && !(u & (1u << 22));
}

int __issignalingl(long double x)
{
    return __issignaling((double)x);
}

/* ------------------------------------------------------------------ */
/* float and long double variants */

#define F1(n) \
    float n##f(float x) { return (float)n((double)x); } \
    long double n##l(long double x) { return n((double)x); }
#define F2(n) \
    float n##f(float x, float y) { return (float)n((double)x, (double)y); } \
    long double n##l(long double x, long double y) { return n((double)x, (double)y); }
#define FN(n) \
    float n##f(float x, long long k) { return (float)n((double)x, k); } \
    long double n##l(long double x, long long k) { return n((double)x, k); }
#define FX(n) \
    float n##f(float x, int r, unsigned w) { return (float)n((double)x, r, w); } \
    long double n##l(long double x, int r, unsigned w) { return n((double)x, r, w); }

F1(acospi) F1(asinpi) F1(atanpi) F2(atan2pi)
F1(cospi) F1(sinpi) F1(tanpi)
F1(exp10) F1(exp10m1) F1(exp2m1) F1(log10p1) F1(log2p1) F1(logp1)
FN(compoundn) FN(pown) F2(powr) FN(rootn) F1(rsqrt)
FX(fromfp) FX(ufromfp) FX(fromfpx) FX(ufromfpx)
