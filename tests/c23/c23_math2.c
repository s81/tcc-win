/* C23 <math.h> functions that tcc-win adds to msvcrt (lib/c23math.c).
   Prints "math2 ok" and exits with 0 on success. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static int err;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); err = 1; } } while (0)

static uint64_t bits(double x) { uint64_t u; memcpy(&u, &x, 8); return u; }
static uint32_t bitsf(float x) { uint32_t u; memcpy(&u, &x, 4); return u; }
static double dbl(uint64_t u) { double x; memcpy(&x, &u, 8); return x; }
static int is_nan(double x) { return (bits(x) << 1) > 0xffe0000000000000ull; }
static int is_nanf(float x) { return (bitsf(x) << 1) > 0xff000000u; }
#define SAME(a, b) (bits(a) == bits(b))     /* also tells -0 from +0 */
#define SAMEF(a, b) (bitsf(a) == bitsf(b))
/* within n ulps (same sign, finite) */
static int near(double a, double b, int n)
{
    int64_t d = (int64_t)(bits(a) - bits(b));
    return (bits(a) >> 63) == (bits(b) >> 63) && d <= n && d >= -n;
}

static double pz, nz, inf, qnan;

static void test_pi(void)
{
    CHECK(SAME(sinpi(pz), pz) && SAME(sinpi(nz), nz));
    CHECK(SAME(sinpi(1.0), pz) && SAME(sinpi(2.0), pz) && SAME(sinpi(-1.0), nz));
    CHECK(SAME(sinpi(-4.0), nz) && SAME(sinpi(1e300), pz));
    CHECK(sinpi(0.5) == 1.0 && sinpi(-0.5) == -1.0 && sinpi(1.5) == -1.0);
    CHECK(sinpi(2.5) == 1.0 && sinpi(-2.5) == -1.0);
    CHECK(near(sinpi(0.25), 0.70710678118654752440, 1));
    CHECK(near(sinpi(1.0 / 6), 0.5, 1));
    CHECK(is_nan(sinpi(inf)) && is_nan(sinpi(-inf)) && is_nan(sinpi(qnan)));

    CHECK(cospi(pz) == 1.0 && cospi(nz) == 1.0 && cospi(1.0) == -1.0);
    CHECK(cospi(2.0) == 1.0 && cospi(-3.0) == -1.0 && cospi(1e300) == 1.0);
    CHECK(SAME(cospi(0.5), pz) && SAME(cospi(-0.5), pz) && SAME(cospi(1.5), pz));
    CHECK(SAME(cospi(2.5), pz) && SAME(cospi(-7.5), pz));
    CHECK(near(cospi(1.0 / 3), 0.5, 1));
    CHECK(is_nan(cospi(inf)) && is_nan(cospi(qnan)));

    CHECK(SAME(tanpi(pz), pz) && SAME(tanpi(nz), nz));
    CHECK(SAME(tanpi(1.0), nz) && SAME(tanpi(2.0), pz));
    CHECK(SAME(tanpi(-1.0), pz) && SAME(tanpi(-2.0), nz));
    CHECK(tanpi(0.5) == inf && tanpi(1.5) == -inf && tanpi(-0.5) == -inf);
    CHECK(tanpi(-1.5) == inf);
    CHECK(tanpi(0.25) == 1.0 && tanpi(-0.25) == -1.0 && tanpi(0.75) == -1.0);
    CHECK(near(tanpi(1.0 / 3), 1.7320508075688772935, 2));
    CHECK(is_nan(tanpi(inf)));

    CHECK(SAME(asinpi(pz), pz) && SAME(asinpi(nz), nz));
    CHECK(asinpi(1.0) == 0.5 && asinpi(-1.0) == -0.5 && is_nan(asinpi(1.5)));
    CHECK(near(asinpi(0.5), 1.0 / 6, 1));
    CHECK(SAME(acospi(1.0), pz) && acospi(-1.0) == 1.0 && acospi(0.0) == 0.5);
    CHECK(near(acospi(0.5), 1.0 / 3, 1) && is_nan(acospi(-2.0)));
    CHECK(SAME(atanpi(pz), pz) && SAME(atanpi(nz), nz));
    CHECK(atanpi(inf) == 0.5 && atanpi(-inf) == -0.5 && atanpi(1.0) == 0.25);
    CHECK(atanpi(-1.0) == -0.25 && is_nan(atanpi(qnan)));
    CHECK(SAME(atan2pi(pz, pz), pz) && SAME(atan2pi(nz, pz), nz));
    CHECK(atan2pi(pz, nz) == 1.0 && atan2pi(nz, nz) == -1.0);
    CHECK(atan2pi(pz, -2.0) == 1.0 && atan2pi(nz, -2.0) == -1.0);
    CHECK(atan2pi(1.0, 0.0) == 0.5 && atan2pi(-1.0, 0.0) == -0.5);
    CHECK(atan2pi(inf, inf) == 0.25 && atan2pi(inf, -inf) == 0.75);
    CHECK(atan2pi(-inf, -inf) == -0.75 && atan2pi(inf, 1.0) == 0.5);
    CHECK(SAME(atan2pi(1.0, inf), pz) && atan2pi(1.0, -inf) == 1.0);
    CHECK(atan2pi(1.0, 1.0) == 0.25 && atan2pi(1.0, -1.0) == 0.75);
    CHECK(atan2pi(-2.0, -2.0) == -0.75 && is_nan(atan2pi(qnan, 1.0)));

    CHECK(sinpif(1.0f) == 0.0f && !signbit(sinpif(1.0f)) && sinpif(0.5f) == 1.0f);
    CHECK(SAMEF(cospif(0.5f), 0.0f) && cospif(1.0f) == -1.0f);
    CHECK(tanpif(0.25f) == 1.0f && asinpif(1.0f) == 0.5f && acospif(-1.0f) == 1.0f);
    CHECK(atanpif(1.0f) == 0.25f && atan2pif(1.0f, -1.0f) == 0.75f);
    CHECK(sinpil(0.5L) == 1.0L && cospil(1.0L) == -1.0L && tanpil(0.25L) == 1.0L);
    CHECK(asinpil(1.0L) == 0.5L && acospil(1.0L) == 0.0L && atanpil(1.0L) == 0.25L);
    CHECK(atan2pil(1.0L, 0.0L) == 0.5L);
}

static void test_exp_log(void)
{
    int i;
    double p = 1.0;
    for (i = 0; i <= 22; i++, p *= 10.0) {
        CHECK(exp10((double)i) == p);
        CHECK(exp10m1((double)i) == p - 1.0);
        CHECK(log10p1(p - 1.0) == (double)i || p - 1.0 + 1.0 != p);
    }
    CHECK(exp10(-1.0) == 0.1 && exp10(-3.0) == 0.001);
    CHECK(near(exp10(0.5), 3.1622776601683793320, 2));
    CHECK(exp10(inf) == inf && SAME(exp10(-inf), pz) && exp10(400.0) == inf);
    CHECK(SAME(exp10m1(pz), pz) && SAME(exp10m1(nz), nz) && exp10m1(-inf) == -1.0);
    CHECK(exp10m1(-1.0) == -0.9 && exp10m1(-2.0) == -0.99 && exp10m1(-30.0) == -1.0);
    CHECK(near(exp10m1(1e-10), 2.3025850932591405e-10, 2));
    CHECK(SAME(exp2m1(pz), pz) && SAME(exp2m1(nz), nz) && exp2m1(-inf) == -1.0);
    CHECK(exp2m1(1.0) == 1.0 && exp2m1(10.0) == 1023.0 && exp2m1(-1.0) == -0.5);
    CHECK(exp2m1(53.0) == 9007199254740991.0 && exp2m1(-2.0) == -0.75);
    CHECK(exp2m1(inf) == inf && exp2m1(2000.0) == inf && exp2m1(-100.0) == -1.0);
    CHECK(near(exp2m1(1e-10), 6.931471805839679e-11, 2));
    CHECK(near(exp2m1(0.5), 0.41421356237309504880, 2));

    CHECK(SAME(logp1(pz), pz) && SAME(logp1(nz), nz) && logp1(-1.0) == -inf);
    CHECK(is_nan(logp1(-2.0)) && logp1(inf) == inf && is_nan(logp1(qnan)));
    CHECK(near(logp1(1e-10), 9.9999999995000000e-11, 2));
    CHECK(near(logp1(1.0), 0.69314718055994530942, 2));
    CHECK(SAME(log2p1(pz), pz) && SAME(log2p1(nz), nz) && log2p1(-1.0) == -inf);
    CHECK(log2p1(1.0) == 1.0 && log2p1(7.0) == 3.0 && log2p1(-0.5) == -1.0);
    CHECK(log2p1(-0.75) == -2.0 && log2p1(1023.0) == 10.0 && is_nan(log2p1(-3.0)));
    CHECK(near(log2p1(2.0), 1.5849625007211561815, 2));
    CHECK(SAME(log10p1(pz), pz) && log10p1(-1.0) == -inf && log10p1(9.0) == 1.0);
    CHECK(log10p1(999.0) == 3.0 && is_nan(log10p1(-1.5)));

    CHECK(exp10f(2.0f) == 100.0f && exp10m1f(1.0f) == 9.0f && exp2m1f(3.0f) == 7.0f);
    CHECK(logp1f(0.0f) == 0.0f && log2p1f(3.0f) == 2.0f && log10p1f(99.0f) == 2.0f);
    CHECK(exp10l(3.0L) == 1000.0L && exp10m1l(2.0L) == 99.0L && exp2m1l(4.0L) == 15.0L);
    CHECK(logp1l(-1.0L) == -inf && log2p1l(1.0L) == 1.0L && log10p1l(9.0L) == 1.0L);
}

static void test_pow(void)
{
    CHECK(compoundn(0.0, 5) == 1.0 && compoundn(1.0, 10) == 1024.0);
    CHECK(compoundn(0.5, 2) == 2.25 && compoundn(1.0, -2) == 0.25);
    CHECK(compoundn(-1.0, 3) == 0.0 && compoundn(-1.0, -3) == inf);
    CHECK(compoundn(qnan, 0) == 1.0 && compoundn(-5.0, 0) == 1.0);
    CHECK(is_nan(compoundn(-2.0, 2)) && compoundn(inf, 2) == inf);
    CHECK(near(compoundn(1e-10, 1000000), 1.0001000050001616, 8));

    CHECK(pown(2.0, 10) == 1024.0 && pown(-2.0, 3) == -8.0 && pown(-3.0, 4) == 81.0);
    CHECK(pown(2.0, -2) == 0.25 && pown(-2.0, -1) == -0.5 && pown(10.0, 22) == 1e22);
    CHECK(pown(qnan, 0) == 1.0 && pown(inf, 0) == 1.0 && pown(0.0, 0) == 1.0);
    CHECK(SAME(pown(nz, 3), nz) && SAME(pown(nz, 2), pz) && SAME(pown(pz, 3), pz));
    CHECK(pown(nz, -3) == -inf && pown(nz, -2) == inf && pown(pz, -1) == inf);
    CHECK(pown(-inf, 3) == -inf && pown(-inf, 2) == inf && SAME(pown(-inf, -3), nz));
    CHECK(SAME(pown(-inf, -2), pz) && pown(2.0, 1024) == inf && pown(2.0, -1074) == dbl(1));
    CHECK(pown(2.0, -1075) == 0.0 && pown(1.0, 1000000000000LL) == 1.0);

    CHECK(powr(2.0, 3.0) == 8.0 && powr(4.0, 0.5) == 2.0 && powr(2.0, -1.0) == 0.5);
    CHECK(is_nan(powr(-2.0, 2.0)) && is_nan(powr(0.0, 0.0)) && is_nan(powr(inf, 0.0)));
    CHECK(is_nan(powr(1.0, inf)) && is_nan(powr(qnan, 0.0)) && is_nan(powr(1.0, qnan)));
    CHECK(powr(1.0, 5.0) == 1.0 && powr(3.0, 0.0) == 1.0 && powr(nz, -1.0) == inf);
    CHECK(SAME(powr(nz, 2.0), pz) && powr(0.0, -inf) == inf && SAME(powr(0.0, inf), pz));
    CHECK(powr(inf, 2.0) == inf && SAME(powr(inf, -2.0), pz) && powr(2.0, inf) == inf);
    CHECK(SAME(powr(0.5, inf), pz) && powr(0.5, -inf) == inf);

    CHECK(rootn(-8.0, 3) == -2.0 && rootn(8.0, 3) == 2.0 && rootn(16.0, 4) == 2.0);
    CHECK(rootn(0.125, 3) == 0.5 && rootn(27.0, -3) == 1.0 / 3 && rootn(4.0, -2) == 0.5);
    CHECK(rootn(1e300, 100) == 1e3 || near(rootn(1e300, 100), 1e3, 1));
    CHECK(rootn(-1.0, 5) == -1.0 && rootn(5.0, 1) == 5.0 && rootn(4.0, -1) == 0.25);
    CHECK(rootn(3486784401.0, 20) == 3.0 && rootn(-1e-300, 3) == -1e-100);
    CHECK(is_nan(rootn(-4.0, 2)) && is_nan(rootn(4.0, 0)) && is_nan(rootn(qnan, 3)));
    CHECK(SAME(rootn(nz, 3), nz) && SAME(rootn(nz, 2), pz) && rootn(nz, -3) == -inf);
    CHECK(rootn(nz, -2) == inf && rootn(inf, 2) == inf && rootn(-inf, 3) == -inf);
    CHECK(SAME(rootn(-inf, -3), nz) && SAME(rootn(inf, -2), pz) && is_nan(rootn(-inf, 2)));

    CHECK(rsqrt(4.0) == 0.5 && rsqrt(0.25) == 2.0 && rsqrt(pz) == inf && rsqrt(nz) == -inf);
    CHECK(SAME(rsqrt(inf), pz) && is_nan(rsqrt(-1.0)) && is_nan(rsqrt(qnan)));
    CHECK(near(rsqrt(2.0), 0.70710678118654752440, 1));

    CHECK(compoundnf(1.0f, 3) == 8.0f && pownf(-2.0f, 5) == -32.0f && powrf(9.0f, 0.5f) == 3.0f);
    CHECK(rootnf(-8.0f, 3) == -2.0f && rsqrtf(16.0f) == 0.25f && is_nanf(rsqrtf(-1.0f)));
    CHECK(compoundnl(0.0L, 5) == 1.0L && pownl(3.0L, 3) == 27.0L && powrl(2.0L, 2.0L) == 4.0L);
    CHECK(rootnl(81.0L, 4) == 3.0L && rsqrtl(4.0L) == 0.5L);
}

static void test_fromfp(void)
{
    CHECK(fromfp(2.5, FP_INT_TONEAREST, 8) == 2.0 && fromfp(3.5, FP_INT_TONEAREST, 8) == 4.0);
    CHECK(fromfp(2.5, FP_INT_TONEARESTFROMZERO, 8) == 3.0);
    CHECK(fromfp(-2.5, FP_INT_TONEARESTFROMZERO, 8) == -3.0);
    CHECK(fromfp(2.7, FP_INT_TOWARDZERO, 8) == 2.0 && fromfp(-2.7, FP_INT_TOWARDZERO, 8) == -2.0);
    CHECK(fromfp(2.1, FP_INT_UPWARD, 8) == 3.0 && fromfp(-2.1, FP_INT_DOWNWARD, 8) == -3.0);
    CHECK(fromfp(127.4, FP_INT_TONEAREST, 8) == 127.0 && is_nan(fromfp(127.6, FP_INT_TONEAREST, 8)));
    CHECK(fromfp(-128.0, FP_INT_TONEAREST, 8) == -128.0 && is_nan(fromfp(-129.0, FP_INT_TONEAREST, 8)));
    CHECK(is_nan(fromfp(1.0, FP_INT_TONEAREST, 0)) && is_nan(fromfp(inf, FP_INT_UPWARD, 64)));
    CHECK(is_nan(fromfp(qnan, FP_INT_UPWARD, 64)) && fromfp(-1.0, FP_INT_TONEAREST, 1) == -1.0);
    CHECK(is_nan(fromfp(1.0, FP_INT_TONEAREST, 1)) && fromfp(1e300, FP_INT_TONEAREST, 2000) == 1e300);
    CHECK(fromfp(9.2e18, FP_INT_TONEAREST, 64) == 9.2e18 && is_nan(fromfp(9.3e18, FP_INT_TONEAREST, 64)));
    CHECK(SAME(fromfp(-0.3, FP_INT_TOWARDZERO, 8), nz));
    CHECK(ufromfp(255.0, FP_INT_TONEAREST, 8) == 255.0 && is_nan(ufromfp(256.0, FP_INT_TONEAREST, 8)));
    CHECK(is_nan(ufromfp(-1.0, FP_INT_TONEAREST, 8)) && ufromfp(-0.4, FP_INT_TONEAREST, 8) == 0.0);
    CHECK(ufromfp(1.5, FP_INT_DOWNWARD, 1) == 1.0 && is_nan(ufromfp(2.0, FP_INT_DOWNWARD, 1)));
    CHECK(fromfpx(2.5, FP_INT_TONEAREST, 8) == 2.0 && ufromfpx(2.5, FP_INT_UPWARD, 8) == 3.0);
    CHECK(fromfpf(-2.5f, FP_INT_TONEAREST, 8) == -2.0f && is_nanf(ufromfpf(3.5f, FP_INT_TONEAREST, 2)));
    CHECK(fromfpxf(7.0f, FP_INT_UPWARD, 4) == 7.0f && ufromfpxf(7.0f, FP_INT_UPWARD, 3) == 7.0f);
    CHECK(fromfpl(-7.5L, FP_INT_DOWNWARD, 4) == -8.0L && ufromfpl(7.5L, FP_INT_TOWARDZERO, 3) == 7.0L);
    CHECK(fromfpxl(1.0L, FP_INT_TONEAREST, 2) == 1.0L && ufromfpxl(0.0L, FP_INT_TONEAREST, 1) == 0.0L);
}

static void test_classify(void)
{
    double d, sub = dbl(1);
    float f, fx = 1.5f;
    long double l, lx = 2.0L;

    CHECK(canonicalize(&d, &pz) == 0 && SAME(d, pz));
    d = 0; CHECK(canonicalize(&d, &nz) == 0 && SAME(d, nz));
    CHECK(canonicalize(&d, &qnan) == 0 && is_nan(d));
    CHECK(canonicalizef(&f, &fx) == 0 && f == 1.5f);
    CHECK(canonicalizel(&l, &lx) == 0 && l == 2.0L);

    CHECK(iscanonical(1.0) && iscanonical(qnan) && iscanonical(1.0f) && iscanonical(1.0L));
    CHECK(!issignaling(1.0) && !issignaling(qnan) && !issignaling(inf));
    CHECK(!issignaling(qnan) && !issignaling((float)qnan) && !issignaling(0.0L));
#ifndef __i386__ /* x87 loads quiet a signaling NaN */
    double snan = dbl(0x7ff0000000000001ull);
    CHECK(issignaling(snan));
    CHECK(canonicalize(&d, &snan) == 0 && is_nan(d) && !issignaling(d));
#endif
    CHECK(issubnormal(sub) && issubnormal(-sub) && !issubnormal(1.0) && !issubnormal(pz));
    CHECK(issubnormal(1e-40f) && !issubnormal(1e-40) && !issubnormal(inf));
    CHECK(iszero(pz) && iszero(nz) && iszero(0.0f) && iszero(0.0L) && !iszero(sub));
    CHECK(!iszero(qnan) && !iszero(1.0f));
    CHECK(FP_INT_UPWARD != FP_INT_DOWNWARD && FP_INT_TONEAREST != FP_INT_TOWARDZERO);
    CHECK(FP_INT_TONEARESTFROMZERO != FP_INT_TONEAREST);
}

int main(void)
{
    pz = 0.0;
    nz = dbl(0x8000000000000000ull);
    inf = dbl(0x7ff0000000000000ull);
    qnan = dbl(0x7ff8000000000000ull);
    test_pi();
    test_exp_log();
    test_pow();
    test_fromfp();
    test_classify();
    if (!err)
        printf("math2 ok\n");
    return err;
}
