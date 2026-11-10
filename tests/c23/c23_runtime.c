/* C23/C11 library functions that tcc-win adds to msvcrt (lib/c23rt.c).
   Prints "runtime ok" and exits with 0 on success. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <time.h>

static int err;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); err = 1; } } while (0)

static uint64_t bits(double x) { uint64_t u; memcpy(&u, &x, 8); return u; }
static uint32_t bitsf(float x) { uint32_t u; memcpy(&u, &x, 4); return u; }
static double dbl(uint64_t u) { double x; memcpy(&x, &u, 8); return x; }
static float flt(uint32_t u) { float x; memcpy(&x, &u, 4); return x; }
static int is_nan(double x) { return (bits(x) << 1) > 0xffe0000000000000ull; }
static int is_nanf(float x) { return (bitsf(x) << 1) > 0xff000000u; }
#define SAME(a, b) (bits(a) == bits(b))     /* also tells -0 from +0 */
#define SAMEF(a, b) (bitsf(a) == bitsf(b))

static void test_string(void)
{
    char buf[8] = "abcdefg", *p;
    CHECK(memset_explicit(buf, 'x', 3) == buf);
    CHECK(strcmp(buf, "xxxdefg") == 0);
    p = strndup("hello", 3);
    CHECK(p && strcmp(p, "hel") == 0);
    free(p);
    p = strndup("hi", 10);
    CHECK(p && strcmp(p, "hi") == 0);
    free(p);
    p = strdup("dup");
    CHECK(p && strcmp(p, "dup") == 0);
    free(p);
}

static void test_stdlib(void)
{
    static _Alignas(64) char a[128];
    char buf[32];
    CHECK(memalignment(NULL) == 0);
    CHECK(memalignment(a) >= 64);
    CHECK(memalignment(a + 1) == 1);
    CHECK(memalignment(a + 8) == 8);
    CHECK(memalignment(a + 48) == 16);

    CHECK(strfromd(buf, sizeof buf, "%.3f", 3.14159) == 5 && strcmp(buf, "3.142") == 0);
    CHECK(strfromd(buf, sizeof buf, "%g", 0.5) == 3 && strcmp(buf, "0.5") == 0);
    CHECK(strfromd(buf, sizeof buf, "%.0F", 2.0) == 1 && strcmp(buf, "2") == 0);
    CHECK(strfromd(buf, 3, "%.2f", 12.5) == 5 && strcmp(buf, "12") == 0);
    CHECK(strfromd(NULL, 0, "%.1f", 1.25) == 3);
    CHECK(strfromd(buf, sizeof buf, "%f", HUGE_VAL) == 3 && strcmp(buf, "inf") == 0);
    CHECK(strfromd(buf, sizeof buf, "%G", -HUGE_VAL) == 4 && strcmp(buf, "-INF") == 0);
    CHECK(strfromd(buf, sizeof buf, "%e", dbl(0x7ff8000000000000ull)) == 3 && strcmp(buf, "nan") == 0);
    CHECK(strfromf(buf, sizeof buf, "%.2e", 1500.0f) == 8 && strcmp(buf, "1.50e+03") == 0);
    CHECK(strfroml(buf, sizeof buf, "%.1f", 0.25L) == 3 && buf[0] == '0');
    CHECK(strfromd(buf, sizeof buf, "%d", 1.0) < 0);
    CHECK(strfromd(buf, sizeof buf, "%5f", 1.0) < 0);
}

static void test_time(void)
{
    struct tm tm, *p;
    struct timespec ts, res;
    time_t t = 86400 * 365 + 3661;       /* 1971-01-01 01:01:01 UTC */

    p = gmtime_r(&t, &tm);
    CHECK(p == &tm && tm.tm_year == 71 && tm.tm_mon == 0 && tm.tm_mday == 1
          && tm.tm_hour == 1 && tm.tm_min == 1 && tm.tm_sec == 1);
    CHECK(timegm(&tm) == t);
    CHECK(localtime_r(&t, &tm) == &tm && tm.tm_year == 70 + (tm.tm_mon == 0));
    CHECK(TIME_UTC == 1);
    CHECK(timespec_get(&ts, TIME_UTC) == TIME_UTC);
    CHECK(ts.tv_sec > 1600000000 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000);
    CHECK(timespec_get(&ts, 0) == 0);
    CHECK(timespec_getres(&res, TIME_UTC) == TIME_UTC);
    CHECK(res.tv_sec == 0 && res.tv_nsec == 100);
    CHECK(timespec_getres(NULL, TIME_UTC) == TIME_UTC);
}

static void test_math(void)
{
    double inf = HUGE_VAL, nan = NAN, mz = -0.0, pz = 0.0;
    float inff = HUGE_VALF, nanf_ = NAN, mzf = -0.0f, pzf = 0.0f;

    CHECK(roundeven(0.5) == 0 && roundeven(1.5) == 2 && roundeven(2.5) == 2);
    CHECK(roundeven(-2.5) == -2 && roundeven(-3.5) == -4 && roundeven(2.6) == 3);
    CHECK(roundeven(1.0) == 1 && roundeven(-1.4) == -1 && roundeven(1e300) == 1e300);
    CHECK(SAME(roundeven(-0.5), mz) && SAME(roundeven(-0.2), mz) && SAME(roundeven(mz), mz));
    CHECK(roundeven(4503599627370495.5) == 4503599627370496.0);
    CHECK(roundeven(inf) == inf && is_nan(roundeven(nan)));
    CHECK(roundevenf(0.5f) == 0 && roundevenf(1.5f) == 2 && roundevenf(-2.5f) == -2);
    CHECK(roundevenf(3.7f) == 4 && SAMEF(roundevenf(-0.4f), mzf) && is_nanf(roundevenf(nanf_)));
    CHECK(roundevenl(2.5L) == 2);

    CHECK(nextup(1.0) == 1.0 + DBL_EPSILON && nextdown(1.0) == 1.0 - DBL_EPSILON / 2);
    CHECK(SAME(nextup(pz), dbl(1)) && SAME(nextup(mz), dbl(1)));
    CHECK(SAME(nextdown(pz), -dbl(1)) && SAME(nextup(-dbl(1)), mz));
    CHECK(nextup(DBL_MAX) == inf && nextup(inf) == inf && nextup(-inf) == -DBL_MAX);
    CHECK(nextdown(-DBL_MAX) == -inf && is_nan(nextup(nan)) && is_nan(nextdown(nan)));
    CHECK(nextupf(1.0f) == 1.0f + FLT_EPSILON && SAMEF(nextupf(0.0f), flt(1)));
    CHECK(nextdownf(1.0f) == 1.0f - FLT_EPSILON / 2 && nextupf(inff) == inff);
    CHECK(nextupl(1.0L) > 1.0L && nextdownl(1.0L) < 1.0L);

    CHECK(fmaximum(1, 2) == 2 && fminimum(1, 2) == 1);
    CHECK(SAME(fmaximum(mz, pz), pz) && SAME(fmaximum(pz, mz), pz));
    CHECK(SAME(fminimum(mz, pz), mz) && SAME(fminimum(pz, mz), mz));
    CHECK(is_nan(fmaximum(nan, 1)) && is_nan(fminimum(1, nan)));
    CHECK(fmaximum_num(nan, 1) == 1 && fminimum_num(1, nan) == 1 && is_nan(fmaximum_num(nan, nan)));
    CHECK(SAME(fmaximum_num(mz, pz), pz) && SAME(fminimum_num(pz, mz), mz));
    CHECK(fmaximum_mag(-3, 2) == -3 && fminimum_mag(-3, 2) == 2);
    CHECK(fmaximum_mag(-2, 2) == 2 && fminimum_mag(-2, 2) == -2);
    CHECK(SAME(fmaximum_mag(mz, pz), pz) && SAME(fminimum_mag(pz, mz), mz));
    CHECK(is_nan(fmaximum_mag(nan, 1)) && is_nan(fminimum_mag(1, nan)));
    CHECK(fmaximum_mag_num(nan, -5) == -5 && fminimum_mag_num(4, nan) == 4);
    CHECK(fmaximum_mag_num(-5, 4) == -5 && fminimum_mag_num(-5, 4) == 4);
    CHECK(is_nan(fminimum_mag_num(nan, nan)));

    CHECK(fmaximumf(1, 2) == 2 && fminimumf(1, 2) == 1 && is_nanf(fmaximumf(nanf_, 1)));
    CHECK(SAMEF(fmaximumf(mzf, pzf), pzf) && SAMEF(fminimumf(pzf, mzf), mzf));
    CHECK(fmaximum_numf(nanf_, 1) == 1 && fminimum_numf(nanf_, 1) == 1);
    CHECK(fmaximum_magf(-3, 2) == -3 && fminimum_magf(-3, 2) == 2);
    CHECK(fmaximum_mag_numf(nanf_, -5) == -5 && fminimum_mag_numf(-5, 4) == 4);
    CHECK(fmaximuml(1, 2) == 2 && fminimuml(1, 2) == 1);
    CHECK(fmaximum_numl(1, 2) == 2 && fminimum_numl(1, 2) == 1);
    CHECK(fmaximum_magl(-3, 2) == -3 && fminimum_magl(-3, 2) == 2);
    CHECK(fmaximum_mag_numl(-3, 2) == -3 && fminimum_mag_numl(-3, 2) == 2);
}

int main(void)
{
    test_string();
    test_stdlib();
    test_time();
    test_math();
    if (!err)
        printf("runtime ok\n");
    return err;
}
