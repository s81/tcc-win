/* Micro-benchmark of lib/decbid.c: operations per second, best of 3 runs,
   for decimal64 and decimal128.  Not part of the test suites.
   Build and run:  tcc -Iinclude tests/decimal/decbench.c -o decbench.exe && decbench.exe
   There is no reference implementation on the build machine, so the numbers
   say nothing about speed relative to other decimal libraries.
   This file is part of tcc-win (LGPL-2.1, see COPYING). */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <tcc_decimal.h>

#define N 1024
static unsigned long long seed = 88172645463325252ULL;
static unsigned long long rnd(void) { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; }
static double now(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart / (double)f.QuadPart; }

static tcc_dec64 a64[N], b64[N], s64[N], r64[N];
static tcc_dec128 a128[N], b128[N], r128[N];
static double dbl[N];
static char str[N][48];
static volatile unsigned long long sink;

/* run body() `reps` times over the N operands; return Mops/s, best of 3 */
#define BENCH(name, reps, body) do { double best = 0; int rep, i, run; \
    for (run = 0; run < 3; run++) { double t0 = now(), t; \
        for (rep = 0; rep < reps; rep++) for (i = 0; i < N; i++) { body; } \
        t = now() - t0; if ((double)reps * N / t > best) best = (double)reps * N / t; } \
    printf("%-26s %8.2f Mops/s\n", name, best / 1e6); } while (0)

int main(void)
{
    tcc_dec_ctx c = { TCC_DEC_NEAREST_EVEN, 0 };
    int i; char buf[64];
    for (i = 0; i < N; i++) {                 /* 16 / 34 digit operands with small exponents */
        unsigned long long x = rnd() % 9000000000000000ULL + 1000000000000000ULL, y = rnd() % 9000000000000000ULL + 1000000000000000ULL;
        sprintf(buf, "%llu", x); strcat(buf, "e-8"); a64[i] = tcc_strtod64(buf, 0);
        sprintf(buf, "%llu", y); strcat(buf, "e-7"); b64[i] = tcc_strtod64(buf, 0);
        sprintf(buf, "%lld.%02d", (long long)(rnd() % 100000), (int)(rnd() % 100)); s64[i] = tcc_strtod64(buf, 0);
        sprintf(buf, "%llu%llu", x, y % 100000000000000000ULL); strcat(buf, "e-20"); a128[i] = tcc_strtod128(buf, 0);
        sprintf(buf, "%llu%llu", y, x % 100000000000000000ULL); strcat(buf, "e-19"); b128[i] = tcc_strtod128(buf, 0);
        dbl[i] = (double)(rnd() % 1000000) / 1024.0 * 3.0;
        tcc_d64_to_string(str[i], 48, a64[i], 0);
    }
    printf("decimal64 (16 digits)\n");
    BENCH("add (rounding)", 200, r64[i] = tcc_d64_add(a64[i], b64[i], &c));
    BENCH("add (exact, money)", 200, r64[i] = tcc_d64_add(s64[i], s64[(i + 1) & (N - 1)], &c));
    BENCH("mul", 200, r64[i] = tcc_d64_mul(a64[i], b64[i], &c));
    BENCH("div", 100, r64[i] = tcc_d64_div(a64[i], b64[i], &c));
    BENCH("sqrt", 20, r64[i] = tcc_d64_sqrt(a64[i], &c));
    BENCH("fma", 100, r64[i] = tcc_d64_fma(a64[i], b64[i], s64[i], &c));
    BENCH("to_string", 100, tcc_d64_to_string(buf, 64, a64[i], 0));
    BENCH("from_string", 100, r64[i] = tcc_d64_from_string(str[i], 0, &c));
    BENCH("from_double", 50, r64[i] = tcc_d64_from_double(dbl[i], &c));
    BENCH("to_double", 100, dbl[i] = tcc_d64_to_double(a64[i], &c));
    BENCH("from_i64", 200, r64[i] = tcc_d64_from_i64((long long)i * 1234567, &c));
    printf("decimal128 (34 digits)\n");
    BENCH("add (rounding)", 100, r128[i] = tcc_d128_add(a128[i], b128[i], &c));
    BENCH("mul", 100, r128[i] = tcc_d128_mul(a128[i], b128[i], &c));
    BENCH("div", 50, r128[i] = tcc_d128_div(a128[i], b128[i], &c));
    BENCH("sqrt", 10, r128[i] = tcc_d128_sqrt(a128[i], &c));
    BENCH("fma", 50, r128[i] = tcc_d128_fma(a128[i], b128[i], a128[(i + 1) & (N - 1)], &c));
    BENCH("to_string", 50, tcc_d128_to_string(buf, 64, a128[i], 0));
    BENCH("from_string", 50, r128[i] = tcc_d128_from_string(str[i], 0, &c));
    BENCH("from_double", 20, r128[i] = tcc_d128_from_double(dbl[i], &c));
    BENCH("to_double", 50, dbl[i] = tcc_d128_to_double(a128[i], &c));
    sink = r64[3].w + r128[3].w[0] + (unsigned long long)dbl[5];
    return 0;
}
