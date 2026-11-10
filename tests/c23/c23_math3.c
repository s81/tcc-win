/* C23 <math.h>: llogb, totalorder, payloads, the narrowing functions,
   and the C99 functions msvcrt lacks (fma, remquo, nearbyint, erf, the
   comparison macros).  NaNs are made and inspected through memory: an
   x87 load would quiet a signaling NaN.  Prints "math3 ok" and exits
   with 0 on success. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

#if defined _WIN32 && __STDC_VERSION_MATH_H__ != 202311L
#error "__STDC_VERSION_MATH_H__"
#endif

static int err;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); err = 1; } } while (0)

static uint64_t bits(double x) { uint64_t u; memcpy(&u, &x, 8); return u; }
static uint32_t bitsf(float x) { uint32_t u; memcpy(&u, &x, 4); return u; }
static uint64_t pbits(const double *p) { uint64_t u; memcpy(&u, p, 8); return u; }
static uint32_t pbitsf(const float *p) { uint32_t u; memcpy(&u, p, 4); return u; }
static void mk(double *p, uint64_t u) { memcpy(p, &u, 8); }
static void mkf(float *p, uint32_t u) { memcpy(p, &u, 4); }
#define SAME(a, b) (bits(a) == bits(b))
#define SAMEF(a, b) (bitsf(a) == bitsf(b))
static int near(double a, double b, int n)
{
    int64_t d = (int64_t)(bits(a) - bits(b));
    return d <= n && d >= -n;
}

static volatile double pz = 0.0, one = 1.0;
static double nz, inf, qnan;

static void test_llogb(void)
{
    CHECK(llogb(pz) == FP_LLOGB0 && llogb(nz) == FP_LLOGB0);
    CHECK(llogb(qnan) == FP_LLOGBNAN && llogbf((float)qnan) == FP_LLOGBNAN);
    CHECK(llogb(inf) == LONG_MAX && llogb(-inf) == LONG_MAX);
    CHECK(llogb(1.0) == 0 && llogb(-0.75) == -1 && llogb(0x1p1023) == 1023);
    CHECK(llogb(0x1p-1074) == -1074 && llogb(0x1.8p-1030) == -1030);
    CHECK(llogbf(0x1p-149f) == -149 && llogbf(0.0f) == FP_LLOGB0);
    CHECK(llogbl(8.0L) == 3);
    CHECK(FP_LLOGB0 == LONG_MIN || FP_LLOGB0 == -LONG_MAX);
}

static void test_totalorder(void)
{
    /* ascending: -qNaN, -sNaN, -inf, -1, -0, +0, min subnormal, 1, inf, +sNaN, +qNaNs */
    static const uint64_t asc[] = {
        0xfff8000000000001ull, 0xfff0000000000001ull, 0xfff0000000000000ull,
        0xbff0000000000000ull, 0x8000000000000000ull, 0x0000000000000000ull,
        0x0000000000000001ull, 0x3ff0000000000000ull, 0x7ff0000000000000ull,
        0x7ff0000000000001ull, 0x7ff8000000000000ull, 0x7ff8000000000001ull,
    };
    static const uint32_t ascf[] = {
        0xffc00000u, 0xff800001u, 0xff800000u, 0x80000000u, 0x00000000u,
        0x7f800000u, 0x7f800001u, 0x7fc00000u,
    };
    int i, j, n = (int)(sizeof asc / sizeof asc[0]);
    double a, b, m1, p1;
    float fa, fb;
    long double la, lb;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            mk(&a, asc[i]);
            mk(&b, asc[j]);
            CHECK((totalorder(&a, &b) != 0) == (i <= j));
        }
    n = (int)(sizeof ascf / sizeof ascf[0]);
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            mkf(&fa, ascf[i]);
            mkf(&fb, ascf[j]);
            CHECK((totalorderf(&fa, &fb) != 0) == (i <= j));
        }
    mk(&a, 0x8000000000000000ull);
    mk(&b, 0);
    CHECK(totalorder(&a, &b) && !totalorder(&b, &a));
    CHECK(totalordermag(&a, &b) && totalordermag(&b, &a));
    m1 = -1.0, p1 = 0.5;
    CHECK(!totalordermag(&m1, &p1) && totalordermag(&p1, &m1));
    mk(&a, 0xfff0000000000001ull);              /* -sNaN */
    CHECK(!totalordermag(&a, &inf) && totalordermag(&inf, &a));
    mkf(&fa, 0xbf800000u), mkf(&fb, 0x3f800000u);   /* -1, 1 */
    CHECK(totalordermagf(&fa, &fb) && totalorderf(&fa, &fb) && !totalorderf(&fb, &fa));
    la = -0.0L, lb = 0.0L;
    CHECK(totalorderl(&la, &lb) && !totalorderl(&lb, &la) && totalordermagl(&lb, &la));
}

static void test_payload(void)
{
    double d, pl;
    float f;
    long double l;
    CHECK(setpayload(&d, 42.0) == 0 && pbits(&d) == 0x7ff800000000002aull);
    CHECK(getpayload(&d) == 42.0);
    CHECK(setpayload(&d, 0.0) == 0 && pbits(&d) == 0x7ff8000000000000ull && getpayload(&d) == 0.0);
    pl = 0x1p51 - 1;
    CHECK(setpayload(&d, pl) == 0 && pbits(&d) == 0x7fffffffffffffffull && getpayload(&d) == pl);
    CHECK(setpayloadsig(&d, 42.0) == 0 && pbits(&d) == 0x7ff000000000002aull);
    CHECK(getpayload(&d) == 42.0);
    CHECK(setpayloadsig(&d, 0.0) != 0 && pbits(&d) == 0);
    CHECK(setpayload(&d, 0x1p51) != 0 && pbits(&d) == 0);
    CHECK(setpayload(&d, 1.5) != 0 && setpayload(&d, -1.0) != 0 && setpayload(&d, inf) != 0);
    CHECK(setpayload(&d, qnan) != 0 && pbits(&d) == 0);
    d = 1.0;
    CHECK(getpayload(&d) == -1.0 && getpayload(&inf) == -1.0);
    CHECK(setpayloadf(&f, 0x3fffff) == 0 && pbitsf(&f) == 0x7fffffffu && getpayloadf(&f) == 0x3fffff);
    CHECK(setpayloadsigf(&f, 7.0f) == 0 && pbitsf(&f) == 0x7f800007u && getpayloadf(&f) == 7.0f);
    CHECK(setpayloadf(&f, 0x1p22f) != 0 && pbitsf(&f) == 0);
    CHECK(setpayloadl(&l, 9.0L) == 0 && getpayloadl(&l) == 9.0L);
    CHECK(setpayloadsigl(&l, 9.0L) == 0 && getpayloadl(&l) == 9.0L);
}

static void test_narrow(void)
{
    double mid = 1 + 0x1p-24;                   /* halfway between two floats */
    /* rounding the double sum to double first would land on mid and then
       round to even (1.0); the single rounding goes the right way */
    CHECK(SAMEF(fadd(mid, 0x1p-80), 1 + 0x1p-23f));
    CHECK(SAMEF(fadd(mid, -0x1p-80), 1.0f));
    CHECK(SAMEF(fadd(0x1p-80, mid), 1 + 0x1p-23f));
    CHECK(SAMEF(fadd(mid, 0.0), 1.0f) && SAMEF(fadd(1 + 0x1.8p-23, 0.0), 1 + 0x1p-22f));
    CHECK(SAMEF(fadd(-mid, -0x1p-80), -1 - 0x1p-23f));
    CHECK(SAMEF(fadd(0x1p100, -0x1p100), 0.0f) && SAMEF(fadd(nz, nz), -0.0f));
    CHECK(SAMEF(fadd(nz, pz), 0.0f) && SAMEF(fadd(3.0, -3.0), 0.0f));
    CHECK(SAMEF(fadd(0x1.ffffffp127, 0.0), (float)inf));     /* ties up to 2^128 */
    CHECK(SAMEF(fadd(0x1.fffffefffffffp127, 0.0), 0x1.fffffep127f));
    CHECK(SAMEF(fadd(0x1p-150, 0x1p-200), 0x1p-149f));       /* subnormal */
    CHECK(SAMEF(fadd(0x1p-150, 0.0), 0.0f));
    CHECK(fadd(inf, 1.0) == (float)inf && isnan(fadd(inf, -inf)) && isnan(fadd(qnan, 1.0)));
    CHECK(SAMEF(fsub(mid, -0x1p-80), 1 + 0x1p-23f) && SAMEF(fsub(1.0, 1.0), 0.0f));
    CHECK(SAMEF(fsub(nz, pz), -0.0f));
    CHECK(SAMEF(faddl(mid, 0x1p-80L), 1 + 0x1p-23f) && SAMEF(fsubl(mid, 0x1p-80L), 1.0f));

    /* 3 * 0x1.555556aaaaaabp-2 is 2^-54 above mid: rounds to mid in double */
    CHECK(SAMEF(fmul(3.0, 0x1.555556aaaaaabp-2), 1 + 0x1p-23f));
    CHECK(SAMEF(fmul(-0x1.555556aaaaaabp-2, 3.0), -1 - 0x1p-23f));
    CHECK(SAMEF(fmul(mid, 1 - 0x1p-53), 1.0f));
    CHECK(SAMEF(fmul(0x1p-149, 0.5), 0.0f) && SAMEF(fmul(0x1p-149, 0x1.000001p-1), 0x1p-149f));
    CHECK(SAMEF(fmul(-1e30, 1e30), (float)-inf) && SAMEF(fmul(-1e-30, 1e-30), -0.0f));
    CHECK(SAMEF(fmull(3.0L, 5.0L), 15.0f) && isnan(fmul(inf, 0.0)));

    CHECK(SAMEF(fdiv(1.0, 3.0), 0x1.555556p-2f) && SAMEF(fdiv(-2.0, 3.0), -0x1.555556p-1f));
    CHECK(SAMEF(fdiv(1.0, 0x1p149), 0x1p-149f) && SAMEF(fdiv(1.0, 0x1p150), 0.0f));
    CHECK(SAMEF(fdiv(1.0, 0x1.fffffffp149), 0x1p-149f));
    CHECK(SAMEF(fdiv(1.0, pz), (float)inf) && SAMEF(fdiv(-1.0, inf), -0.0f));
    CHECK(SAMEF(fdivl(1.0L, 3.0L), 0x1.555556p-2f) && SAMEF(fdiv(1e300, 1e-300), (float)inf));

    CHECK(SAMEF(ffma(mid, 1.0, 0x1p-80), 1 + 0x1p-23f));
    CHECK(SAMEF(ffma(mid, -1.0, 0x1p-80), -1.0f));
    CHECK(SAMEF(ffma(1 + 0x1p-30, 1 + 0x1p-30, -1.0), 0x1p-29f));
    CHECK(SAMEF(ffma(0x1p600, 0x1p600, -inf), (float)-inf));
    CHECK(SAMEF(ffma(2.0, 3.0, -6.0), 0.0f) && SAMEF(ffma(nz, 1.0, nz), -0.0f));
    CHECK(SAMEF(ffma(1e300, 1e300, -1e300), (float)inf) && isnan(ffma(inf, 0.0, 1.0)));
    CHECK(SAMEF(ffmal(mid, 1.0L, 0x1p-80L), 1 + 0x1p-23f));

    /* sqrt is just below 1 + 2^-24 + 2^-53: double then float gives 1 */
    CHECK(SAMEF(fsqrt(0x1.0000020000011p+0), 1 + 0x1p-23f));
    CHECK(SAMEF(fsqrt(2.0), 0x1.6a09e6p+0f) && SAMEF(fsqrt(0x1p-298), 0x1p-149f));
    CHECK(SAMEF(fsqrt(nz), -0.0f) && isnan(fsqrt(-1.0)) && fsqrt(inf) == (float)inf);
    CHECK(SAMEF(fsqrtl(16.0L), 4.0f));

    CHECK(SAME(daddl(0.1L, 0.2L), 0.1 + 0.2) && SAME(dsubl(0.1L, 0.3L), 0.1 - 0.3));
    CHECK(SAME(dmull(0.1L, 3.0L), 0.1 * 3.0) && SAME(ddivl(1.0L, 3.0L), 1.0 / 3.0));
    CHECK(SAME(dmull(0x1p-1000L, 0x1p-74L), 0x1p-1074) && SAME(dmull(0x1p-1000L, 0x1p-75L), 0.0));
    CHECK(SAME(ddivl(0x1.8p-1073L, 2.0L), 0x1p-1073));       /* tie to even */
    CHECK(SAME(dfmal(1 + 0x1p-27L, 1 + 0x1p-27L, -(1 + 0x1p-26L)), 0x1p-54));
    CHECK(SAME(dsqrtl(2.0L), 0x1.6a09e667f3bcdp+0) && SAME(dsqrtl(0x1p-1074L), 0x1p-537));
}

static void test_c99(void)
{
    int q;
    double a = 1 + 0x1p-27;
    CHECK(SAME(fma(a, a, -(1 + 0x1p-26)), 0x1p-54) && SAME(fma(2.0, 3.0, -6.0), 0.0));
    CHECK(SAME(fma(0x1p-1022, 0x1p-52, 0.0), 0x1p-1074) && SAME(fma(0x1p1023, 2.0, -0x1p1023), 0x1p1023));
    CHECK(SAME(fma(1e308, 10.0, -inf), -inf) && SAME(fmal(2.0L, 3.0L, 1.0L), 7.0));
    CHECK(SAMEF(fmaf(1 + 0x1p-12f, 1 + 0x1p-12f, -(1 + 0x1p-11f)), 0x1p-24f));
    CHECK(remquo(10.0, 3.0, &q) == 1.0 && (q & 7) == 3);
    CHECK(remquo(11.0, 3.0, &q) == -1.0 && (q & 7) == 4);
    CHECK(remquo(-7.0, 2.0, &q) == 1.0 && q < 0 && (-q & 7) == 4);
    CHECK(SAME(remquo(-6.0, 2.0, &q), -0.0) && q == -3);
    CHECK(remquo(1.0, 0x1p-1074, &q) == 0.0 && isnan(remquo(1.0, 0.0, &q)));
    CHECK(remainder(5.0, 2.0) == 1.0 && remainder(7.0, 2.0) == -1.0);
    CHECK(remainder(0x1p1000, 3.0) == 1.0 && remainder(1.0, inf) == 1.0);
    CHECK(remainderf(5.5f, 2.0f) == -0.5f && remainderl(4.0L, 3.0L) == 1.0L);
    CHECK(SAME(remainder(0x1.8p-1073, 0x1p-1073), -0x1p-1074));
    CHECK(nearbyint(2.5) == 2.0 && nearbyint(3.5) == 4.0 && SAME(nearbyint(-0.4), -0.0));
    CHECK(nearbyint(0x1p60) == 0x1p60 && nearbyintf(-1.5f) == -2.0f && nearbyintl(0.6L) == 1.0L);
    CHECK(SAME(erf(pz), pz) && SAME(erf(nz), nz) && erf(inf) == 1.0 && erf(-inf) == -1.0);
    CHECK(near(erf(1.0), 0.8427007929497149, 2) && near(erf(-0.5), -0.5204998778130465, 2));
    CHECK(erfc(inf) == 0.0 && erfc(-inf) == 2.0 && erfc(0.0) == 1.0);
    CHECK(near(erfc(10.0), 2.088487583762545e-45, 8) && near(erfc(0.6), 0.3961439091520741, 16));
    CHECK(erff(1.0f) == 0x1.af767ap-1f && erfcl(30.0L) == 0.0L);
    CHECK(SAME(atan2l(0.0L, -1.0L), atan2(0.0, -1.0)) && SAME(copysignl(2.0L, -0.0L), -2.0));
    CHECK(isless(1.0, 2.0) && !isless(1.0, qnan) && !isgreater(qnan, 1.0));
    CHECK(isgreaterequal(2.0f, 2.0) && islessequal(-inf, 0.0) && islessgreater(1.0, 2.0));
    CHECK(!islessgreater(1.0, 1.0) && isunordered(1.0, qnan) && !isunordered(1.0, 2.0));
    CHECK(iseqsig(1.0, 1.0) && !iseqsig(1.0, qnan));
    CHECK((math_errhandling & (MATH_ERRNO | MATH_ERREXCEPT)) != 0);
    CHECK(sizeof(float_t) >= sizeof(float) && sizeof(double_t) >= sizeof(double));
}

int main(void)
{
    nz = -pz, inf = one / pz, qnan = pz / pz;
    test_llogb();
    test_totalorder();
    test_payload();
    test_narrow();
    test_c99();
    if (err)
        return 1;
    printf("math3 ok\n");
    return 0;
}
