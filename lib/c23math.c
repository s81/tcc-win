/* C23 <math.h> functions that msvcrt.dll lacks (part of libtcc1.a on
   Windows targets), in addition to those in c23rt.c:

   acospi asinpi atanpi atan2pi cospi sinpi tanpi
   exp10 exp10m1 exp2m1 log10p1 log2p1 logp1
   compoundn pown powr rootn rsqrt
   fromfp ufromfp fromfpx ufromfpx canonicalize llogb
   totalorder totalordermag getpayload setpayload setpayloadsig
   (each also with f and l suffix), __issignaling{,f,l} and __iseqsig
   for the issignaling and iseqsig macros; the narrowing fadd fsub fmul
   fdiv ffma fsqrt (also with l suffix) and daddl dsubl dmull ddivl dfmal
   dsqrtl; and the C99 fma remquo remainder nearbyint erf erfc (with f
   and l suffix), atan2l and copysignl.

   The narrowing functions and fma are correctly rounded (to nearest;
   the dynamic rounding mode is ignored), computed in integers.

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
/* llogb, totalorder, payloads */

#define LLOGB_MAX ((long)(~0ul >> 1))
#define LLOGB_MIN (-LLOGB_MAX - 1)      /* FP_LLOGB0, FP_LLOGBNAN */

long llogb(double x)
{
    uint64_t u = d2u(x) & 0x7fffffffffffffffull;
    int e = (int)(u >> 52);
    if (u == 0 || d_isnan(x))
        return LLOGB_MIN;
    if (e == 0x7ff)
        return LLOGB_MAX;
    if (e == 0) {                       /* subnormal */
        e = -1022;
        while (!(u & (1ull << 52)))
            u <<= 1, e--;
        return e;
    }
    return e - 1023;
}
long llogbf(float x) { return llogb((double)x); }
long llogbl(long double x) { return llogb((double)x); }

/* sign-magnitude bits to an unsigned key that orders like totalOrder */
static uint64_t to_key(uint64_t u) { return u >> 63 ? ~u : u | 0x8000000000000000ull; }

/* the operands are read as bits: no floating-point load quiets them */
int totalorder(const double *x, const double *y)
{
    uint64_t a, b;
    memcpy(&a, x, 8);
    memcpy(&b, y, 8);
    return to_key(a) <= to_key(b);
}
int totalorderf(const float *x, const float *y)
{
    uint32_t a, b;
    memcpy(&a, x, 4);
    memcpy(&b, y, 4);
    return to_key((uint64_t)a << 32) <= to_key((uint64_t)b << 32);
}
int totalorderl(const long double *x, const long double *y)
{
    return totalorder((const double *)x, (const double *)y);
}
int totalordermag(const double *x, const double *y)
{
    uint64_t a, b;
    memcpy(&a, x, 8);
    memcpy(&b, y, 8);
    return (a & 0x7fffffffffffffffull) <= (b & 0x7fffffffffffffffull);
}
int totalordermagf(const float *x, const float *y)
{
    uint32_t a, b;
    memcpy(&a, x, 4);
    memcpy(&b, y, 4);
    return (a & 0x7fffffffu) <= (b & 0x7fffffffu);
}
int totalordermagl(const long double *x, const long double *y)
{
    return totalordermag((const double *)x, (const double *)y);
}

/* payload: the significand bits below the quiet bit; -1 if not a NaN */
double getpayload(const double *x)
{
    uint64_t u;
    memcpy(&u, x, 8);
    if ((u << 1) <= 0xffe0000000000000ull)
        return -1.0;
    return (double)(u & ((1ull << 51) - 1));
}
float getpayloadf(const float *x)
{
    uint32_t u;
    memcpy(&u, x, 4);
    if ((u << 1) <= 0xff000000u)
        return -1.0f;
    return (float)(u & ((1u << 22) - 1));
}
long double getpayloadl(const long double *x) { return getpayload((const double *)x); }

/* pl as an integer below 2^bits, or -1 */
static int64_t payload_int(double pl, int bits)
{
    uint64_t n;
    if (d_neg(pl) || !(pl < d_pow2(bits)))
        return -1;
    n = (uint64_t)pl;
    return (double)n == pl ? (int64_t)n : -1;
}

static int set_payload(double *res, double pl, int sig)
{
    int64_t n = payload_int(pl, 51);
    uint64_t u = 0;
    int r = 1;
    if (n > 0 || (n == 0 && !sig)) {
        u = 0x7ff0000000000000ull | (uint64_t)n | (sig ? 0 : 1ull << 51);
        r = 0;
    }
    memcpy(res, &u, 8);
    return r;
}

static int set_payloadf(float *res, float pl, int sig)
{
    int64_t n = payload_int(pl, 22);
    uint32_t u = 0;
    int r = 1;
    if (n > 0 || (n == 0 && !sig)) {
        u = 0x7f800000u | (uint32_t)n | (sig ? 0 : 1u << 22);
        r = 0;
    }
    memcpy(res, &u, 4);
    return r;
}

int setpayload(double *res, double pl) { return set_payload(res, pl, 0); }
int setpayloadsig(double *res, double pl) { return set_payload(res, pl, 1); }
int setpayloadf(float *res, float pl) { return set_payloadf(res, pl, 0); }
int setpayloadsigf(float *res, float pl) { return set_payloadf(res, pl, 1); }
int setpayloadl(long double *res, long double pl) { return set_payload((double *)res, pl, 0); }
int setpayloadsigl(long double *res, long double pl) { return set_payload((double *)res, pl, 1); }

/* ------------------------------------------------------------------ */
/* Correctly rounded arithmetic in integers: the narrowing functions
   (fadd ... fsqrt, daddl ... dsqrtl) and fma.  The exact result is formed
   as a 128-bit integer times a power of two (plus a sticky bit for
   nonzero bits beyond it) and rounded once, to nearest even, to 24 or 53
   bits with the exponent range of float or double.  Uses no
   floating-point arithmetic, so neither the x87 nor msvcrt matters. */

typedef struct { uint64_t hi, lo; } u128;

static int top64(uint64_t x)
{
    int n = 63;
    while (!(x >> n))
        n--;
    return n;
}

static int top128(u128 a)
{
    return a.hi ? 64 + top64(a.hi) : a.lo ? top64(a.lo) : -1;
}

static u128 u_shl(u128 a, int n)               /* 0 <= n < 128 */
{
    u128 r;
    if (n == 0)
        return a;
    if (n >= 64) {
        r.hi = a.lo << (n - 64);
        r.lo = 0;
    } else {
        r.hi = a.hi << n | a.lo >> (64 - n);
        r.lo = a.lo << n;
    }
    return r;
}

/* bits [0, n) of a nonzero */
static int u_low(u128 a, int n)
{
    if (n <= 0)
        return 0;
    if (n >= 128)
        return (a.hi | a.lo) != 0;
    if (n >= 64)
        return a.lo != 0 || (n > 64 && (a.hi << (128 - n)) != 0);
    return (a.lo << (64 - n)) != 0;
}

static u128 u_shr(u128 a, int n, int *sticky)  /* n >= 0 */
{
    u128 r;
    *sticky |= u_low(a, n);
    if (n >= 128) {
        r.hi = r.lo = 0;
    } else if (n >= 64) {
        r.hi = 0;
        r.lo = n == 64 ? a.hi : a.hi >> (n - 64);
    } else if (n == 0) {
        r = a;
    } else {
        r.hi = a.hi >> n;
        r.lo = a.lo >> n | a.hi << (64 - n);
    }
    return r;
}

static u128 u_add(u128 a, u128 b)
{
    u128 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo);
    return r;
}

static u128 u_sub(u128 a, u128 b)              /* a >= b */
{
    u128 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo);
    return r;
}

static int u_cmp(u128 a, u128 b)
{
    if (a.hi != b.hi)
        return a.hi < b.hi ? -1 : 1;
    return a.lo < b.lo ? -1 : a.lo > b.lo;
}

static u128 u_mul(uint64_t a, uint64_t b)
{
    uint64_t al = a & 0xffffffffu, ah = a >> 32, bl = b & 0xffffffffu, bh = b >> 32;
    uint64_t ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    uint64_t mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
    u128 r;
    r.lo = (mid << 32) | (ll & 0xffffffffu);
    r.hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
    return r;
}

static u128 u_make(uint64_t hi, uint64_t lo)
{
    u128 r;
    r.hi = hi;
    r.lo = lo;
    return r;
}

/* q * 2^k as a double, exact by construction (q < 2^54) */
static double make_double(int neg, uint64_t q, int k)
{
    uint64_t s = (uint64_t)neg << 63, m;
    int t, e;
    if (q == 0)
        return u2d(s);
    t = top64(q);
    e = t + k;
    if (e > 1023)
        return u2d(s | 0x7ff0000000000000ull);
    if (e >= -1022) {
        m = t > 52 ? q >> (t - 52) : q << (52 - t);
        return u2d(s | (uint64_t)(e + 1023) << 52 | (m & ((1ull << 52) - 1)));
    }
    e = k + 1074;
    return u2d(s | (e >= 0 ? q << e : q >> -e));
}

/* (m + f) * 2^e, f in (0, 1) if sticky else 0, rounded to nearest even
   with prec (24 or 53) bits in the range of float or double */
static double round_to(int neg, u128 m, int sticky, int e, int prec)
{
    int emin = prec == 24 ? -126 : -1022, emax = prec == 24 ? 127 : 1023;
    int t = top128(m), sh, z = 0;
    uint64_t q;
    if (t < 0)
        return make_double(neg, 0, 0);
    sh = t + e >= emin ? t - (prec - 1) : emin - (prec - 1) - e;
    if (sh <= 0)
        return make_double(neg, u_shl(m, -sh).lo, e + sh);
    if (sh > 128)
        return make_double(neg, 0, 0);
    q = u_shr(m, sh, &z).lo;
    if ((u_shr(m, sh - 1, &z).lo & 1)           /* half bit */
        && (u_low(m, sh - 1) || sticky || (q & 1)))
        q++;
    if (top64(q | 1) + e + sh > emax)
        return make_double(neg, 1, 1024);       /* infinity */
    return make_double(neg, q, e + sh);
}

/* finite nonzero x = m * 2^e with bit 52 of m set; returns the sign */
static int split(double x, uint64_t *m, int *e)
{
    uint64_t u = d2u(x);
    int b = (int)(u >> 52 & 0x7ff);
    *m = u & ((1ull << 52) - 1);
    if (b) {
        *m |= 1ull << 52;
        *e = b - 1075;
    } else {
        *e = -1074;
        while (!(*m & (1ull << 52)))
            *m <<= 1, --*e;
    }
    return (int)(u >> 63);
}

/* a, b aligned to the same scale: sa*(a) + sb*(b) * 2^-d (b shifted d
   bits right), |a| >= |b * 2^-d|; rounded */
static double add_aligned(int sa, u128 a, int sb, u128 b, int d, int e, int prec)
{
    int st = 0;
    u128 r;
    b = u_shr(b, d, &st);
    if (sa == sb)
        return round_to(sa, u_add(a, b), st, e, prec);
    r = u_sub(a, b);
    if (st)                                     /* a - b - f = (r - 1) + (1 - f) */
        r = u_sub(r, u_make(0, 1));
    if (!st && (r.hi | r.lo) == 0)
        return 0.0;
    return round_to(sa, r, st, e, prec);
}

/* x, y finite nonzero */
static double r_add(double x, double y, int prec)
{
    uint64_t mx, my;
    int ex, ey, sx = split(x, &mx, &ex), sy = split(y, &my, &ey);
    if (ex < ey || (ex == ey && mx < my))
        return add_aligned(sy, u_make(my, 0), sx, u_make(mx, 0), ey - ex, ey - 64, prec);
    return add_aligned(sx, u_make(mx, 0), sy, u_make(my, 0), ex - ey, ex - 64, prec);
}

static double r_mul(double x, double y, int prec)
{
    uint64_t mx, my;
    int ex, ey, sx = split(x, &mx, &ex), sy = split(y, &my, &ey);
    return round_to(sx ^ sy, u_mul(mx, my), 0, ex + ey, prec);
}

static double r_div(double x, double y, int prec)
{
    uint64_t mx, my, q = 0;
    int ex, ey, i, sx = split(x, &mx, &ex), sy = split(y, &my, &ey);
    for (i = 0; i <= 62; i++) {                 /* q = floor(mx * 2^62 / my) */
        q <<= 1;
        if (mx >= my)
            mx -= my, q |= 1;
        mx <<= 1;
    }
    return round_to(sx ^ sy, u_make(0, q), mx != 0, ex - ey - 62, prec);
}

/* x, y, z finite nonzero */
static double r_fma(double x, double y, double z, int prec)
{
    uint64_t mx, my, mz;
    int ex, ey, ez, sh, sx = split(x, &mx, &ex), sy = split(y, &my, &ey);
    int sz = split(z, &mz, &ez);
    u128 p = u_mul(mx, my), c = u_make(mz << 9, 0);  /* top bits at 125 */
    sh = 125 - top128(p);
    p = u_shl(p, sh);
    ex += ey - sh;
    ez -= 73;
    if (ex > ez || (ex == ez && u_cmp(p, c) >= 0))
        return add_aligned(sx ^ sy, p, sz, c, ex - ez, ex, prec);
    return add_aligned(sz, c, sx ^ sy, p, ez - ex, ez, prec);
}

/* x finite > 0 */
static double r_sqrt(double x, int prec)
{
    uint64_t mx, r = 0, c;
    int ex, b, s;
    u128 m, sq;
    split(x, &mx, &ex);
    s = (ex - 72) & 1 ? 73 : 72;                /* even exponent, top at 124/125 */
    m = u_shl(u_make(0, mx), s);
    for (b = 63; b >= 0; b--) {
        c = r | 1ull << b;
        if (u_cmp(u_mul(c, c), m) <= 0)
            r = c;
    }
    sq = u_mul(r, r);
    return round_to(0, u_make(0, r), u_cmp(sq, m) != 0, (ex - s) / 2, prec);
}

static int d_fin(double x) { return (d2u(x) << 1) < 0xffe0000000000000ull; }

static double n_add(double x, double y, int prec)
{
    if (!d_fin(x) || !d_fin(y) || x == 0 || y == 0)
        return x + y;                           /* exact or special */
    return r_add(x, y, prec);
}

static double n_mul(double x, double y, int prec)
{
    if (!d_fin(x) || !d_fin(y) || x == 0 || y == 0)
        return x * y;
    return r_mul(x, y, prec);
}

static double n_div(double x, double y, int prec)
{
    if (!d_fin(x) || !d_fin(y) || x == 0 || y == 0)
        return x / y;
    return r_div(x, y, prec);
}

static double n_fma(double x, double y, double z, int prec)
{
    if (!d_fin(x) || !d_fin(y))
        return x * y + z;
    if (!d_fin(z))
        return z + z;                   /* finite + inf, or a NaN */
    if (x == 0 || y == 0)
        return n_add(x * y, z, prec);   /* x * y is an exact zero */
    if (z == 0)
        return r_mul(x, y, prec);
    return r_fma(x, y, z, prec);
}

static double n_sqrt(double x, int prec)
{
    if (d_isnan(x))
        return x + x;
    if (x == 0 || (d_isinf(x) && !d_neg(x)))
        return x;
    if (d_neg(x))
        return d_nan();
    return r_sqrt(x, prec);
}

#define NARROW2(n, op) \
    float f##n(double x, double y) { return (float)op(x, y, 24); } \
    float f##n##l(long double x, long double y) { return (float)op(x, y, 24); } \
    double d##n##l(long double x, long double y) { return op(x, y, 53); }
static double n_sub(double x, double y, int prec) { return n_add(x, -y, prec); }
NARROW2(add, n_add)
NARROW2(sub, n_sub)
NARROW2(mul, n_mul)
NARROW2(div, n_div)
#undef NARROW2

float ffma(double x, double y, double z) { return (float)n_fma(x, y, z, 24); }
float ffmal(long double x, long double y, long double z) { return (float)n_fma(x, y, z, 24); }
double dfmal(long double x, long double y, long double z) { return n_fma(x, y, z, 53); }
float fsqrt(double x) { return (float)n_sqrt(x, 24); }
float fsqrtl(long double x) { return (float)n_sqrt(x, 24); }
double dsqrtl(long double x) { return n_sqrt(x, 53); }

/* C99 functions that msvcrt.dll lacks */
double fma(double x, double y, double z) { return n_fma(x, y, z, 53); }
float fmaf(float x, float y, float z) { return (float)n_fma(x, y, z, 24); }
long double fmal(long double x, long double y, long double z) { return n_fma(x, y, z, 53); }

/* exact; quo gets the low 31 bits of the quotient's magnitude */
double remquo(double x, double y, int *quo)
{
    uint64_t mx, my, q = 0;
    int ex, ey, k, sx, sy, neg = 0;
    double r, ay;
    *quo = 0;
    if (d_isnan(x) || d_isnan(y))
        return x + y;
    if (d_isinf(x) || y == 0)
        return d_nan();
    if (d_isinf(y) || x == 0)
        return x;
    sx = split(x, &mx, &ex);
    sy = split(y, &my, &ey);
    if (ex < ey) {                              /* |x| < |y| */
        r = d_abs(x);
        ay = d_abs(y);
        if (r > ay - r) {                       /* r - ay exact (Sterbenz) */
            r -= ay;
            *quo = sx ^ sy ? -1 : 1;
        }
        return sx ? -r : r;
    }
    for (k = ex - ey; k > 0; k--) {
        if (mx >= my)
            mx -= my, q++;
        mx <<= 1;
        q <<= 1;
    }
    if (mx >= my)
        mx -= my, q++;
    if (2 * mx > my || (2 * mx == my && (q & 1)))
        mx = my - mx, q++, neg = 1;
    *quo = (int)(q & 0x7fffffff) * (sx ^ sy ? -1 : 1);
    return make_double(sx ^ neg, mx, ey);
}
float remquof(float x, float y, int *quo) { return (float)remquo(x, y, quo); }
long double remquol(long double x, long double y, int *quo) { return remquo(x, y, quo); }
double remainder(double x, double y) { int q; return remquo(x, y, &q); }
float remainderf(float x, float y) { int q; return (float)remquo(x, y, &q); }
long double remainderl(long double x, long double y) { int q; return remquo(x, y, &q); }

/* round to an integer in the current rounding mode: adding and
   subtracting 2^52 leaves no fraction bits */
double nearbyint(double x)
{
    volatile double t = x;
    if (!(d_abs(x) < 0x1p52))
        return d_isnan(x) ? x + x : x;          /* integral, inf or NaN */
    if (x > 0) {
        t += 0x1p52;
        t -= 0x1p52;
    } else {
        t -= 0x1p52;
        t += 0x1p52;
    }
    return d_copysign(t, x);
}
float nearbyintf(float x) { return (float)nearbyint(x); }
long double nearbyintl(long double x) { return nearbyint(x); }

/* exp(-a^2), with a^2 split exactly; a < 27 */
static double expmsq(double a)
{
    double h, l;
    two_prod(a, a, &h, &l);
    return exp(-h) * (1.0 - l);
}

#define TWO_SQRTPI 1.12837916709551257390      /* 2/sqrt(pi) */

/* erf(a) for 0 <= a < 1: Taylor series */
static double erf_series(double a)
{
    double a2 = a * a, p = a, s = 0.0, t[40];
    int n, k = 0;
    for (n = 1; n < 40; n++) {          /* p = (-1)^n a^(2n+1) / n! */
        t[k++] = p / (2 * n - 1);
        p *= -a2 / n;
        if (d_abs(p) < 1e-18 * a)
            break;
    }
    while (k > 0)                       /* smallest first */
        s += t[--k];
    return TWO_SQRTPI * s;
}

/* erfc(a) for a >= 0.5: continued fraction
   erfc(a) = exp(-a^2) / sqrt(pi) / (a + (1/2)/(a + 1/(a + (3/2)/(a + ...))));
   the length is found by Lentz's method, the value evaluated backward */
static double erfc_cf(double a)
{
    double c = a, d = 0.0, e, t;
    int n;
    if (a > 27.3)
        return 0.0;
    for (n = 1; n < 100000; n++) {
        d = 1.0 / (a + 0.5 * n * d);
        c = a + 0.5 * n / c;
        e = c * d - 1.0;
        if (e < 1e-18 && e > -1e-18)
            break;
    }
    for (t = a, n += 4; n > 0; n--)
        t = a + 0.5 * n / t;
    return 0.5 * TWO_SQRTPI * expmsq(a) / t;
}

double erf(double x)
{
    double a = d_abs(x), r;
    if (d_isnan(x))
        return x + x;
    if (a < 0x1p-28)
        return x + 0.12837916709551257390 * x;
    if (a < 1.0)
        r = erf_series(a);
    else if (a < 6.0)
        r = 1.0 - erfc_cf(a);
    else
        r = 1.0;
    return d_neg(x) ? -r : r;
}

double erfc(double x)
{
    if (d_isnan(x))
        return x + x;
    if (x < 0.5)
        return 1.0 - erf(x);
    return erfc_cf(x);
}

float erff(float x) { return (float)erf(x); }
long double erfl(long double x) { return erf(x); }
float erfcf(float x) { return (float)erfc(x); }
long double erfcl(long double x) { return erfc(x); }

long double atan2l(long double y, long double x) { return atan2(y, x); }
long double copysignl(long double x, long double y) { return d_copysign(x, y); }

int __iseqsig(double x, double y) { return x == y; }

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
