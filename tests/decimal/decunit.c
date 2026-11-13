/* Unit tests for the parts of lib/decbid.c that the vector file does not
   reach: the per-thread default context, strtod names, a few hand-checked
   values.  Runs as an executable or under -run.
   This file is part of tcc-win (LGPL-2.1, see COPYING). */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <tcc_decimal.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static char *str64(tcc_dec64 x) { static char b[8][64]; static int i; char *r = b[i++ & 7]; tcc_d64_to_string(r, 64, x, 0); return r; }

static volatile int thread_ok[4];
static DWORD WINAPI worker(LPVOID arg)
{
    int id = (int)(size_t)arg, i;
    tcc_dec_ctx *c = tcc_dec_thread_ctx();
    c->rounding = id;                          /* each thread picks its own mode */
    c->flags = 0;
    for (i = 0; i < 2000; i++) {
        tcc_dec64 one = tcc_d64_from_i64(1, 0), three = tcc_d64_from_i64(3, 0);
        tcc_dec64 q = tcc_d64_div(one, three, 0);     /* NULL: this thread's context */
        if (c->rounding != id || !(c->flags & TCC_DEC_INEXACT)) return 1;
        c->flags = 0;
        /* 1/3 = 0.3333333333333333(33..): toward +inf must end in ...334 */
        char b[64];
        tcc_d64_to_string(b, 64, q, 0);
        if (id == TCC_DEC_UPWARD && strcmp(b, "0.3333333333333334")) return 2;
        if (id != TCC_DEC_UPWARD && strcmp(b, "0.3333333333333333")) return 3;
    }
    thread_ok[id] = 1;
    return 0;
}

int main(void)
{
    int i; HANDLE h[4]; tcc_dec_ctx *main_ctx = tcc_dec_thread_ctx(); char *e;
    tcc_dec_ctx c = { TCC_DEC_NEAREST_EVEN, 0 };

    CHECK(main_ctx == tcc_dec_thread_ctx());
    CHECK(main_ctx->rounding == TCC_DEC_NEAREST_EVEN && main_ctx->flags == 0);
    for (i = 0; i < 4; i++) h[i] = CreateThread(0, 0, worker, (LPVOID)(size_t)i, 0, 0);
    WaitForMultipleObjects(4, h, 1, INFINITE);
    for (i = 0; i < 4; i++) { DWORD rc = 99; GetExitCodeThread(h[i], &rc); CHECK(rc == 0 && thread_ok[i]); CloseHandle(h[i]); }
    CHECK(main_ctx->rounding == TCC_DEC_NEAREST_EVEN && main_ctx->flags == 0);   /* untouched by the threads */

    /* the NULL context records flags in the thread context */
    (void)tcc_d64_div(tcc_d64_from_i64(1, 0), tcc_d64_from_i64(0, 0), 0);
    CHECK(main_ctx->flags == TCC_DEC_DIVBYZERO);
    main_ctx->flags = 0;

    /* strtod names, trailing text, whitespace */
    CHECK(!strcmp(str64(tcc_strtod64("  -12.50e2xyz", &e)), "-1250") && !strcmp(e, "xyz"));
    CHECK(!strcmp(str64(tcc_strtod64("0.1", 0)), "0.1"));
    { tcc_dec32 d = tcc_strtod32("9.9999995", 0); char b[32]; tcc_d32_to_string(b, 32, d, 0); CHECK(!strcmp(b, "10.00000")); }
    { tcc_dec128 d = tcc_strtod128("1e-6176", 0); char b[64]; tcc_d128_to_string(b, 64, d, 0); CHECK(!strcmp(b, "1E-6176")); }
    CHECK(main_ctx->flags == TCC_DEC_INEXACT);   /* 9.9999995 rounded; nothing else raised */
    main_ctx->flags = 0;

    /* exact sums keep the cohort, 0.1 + 0.2 is exact in decimal */
    CHECK(!strcmp(str64(tcc_d64_add(tcc_strtod64("0.1", 0), tcc_strtod64("0.2", 0), &c)), "0.3") && !c.flags);
    CHECK(!strcmp(str64(tcc_d64_mul(tcc_strtod64("1.10", 0), tcc_strtod64("1.10", 0), &c)), "1.2100") && !c.flags);
    CHECK(!strcmp(str64(tcc_d64_sqrt(tcc_strtod64("2", 0), &c)), "1.414213562373095") && c.flags == TCC_DEC_INEXACT);
    c.flags = 0;
    CHECK(tcc_d64_compare(tcc_strtod64("1.0", 0), tcc_strtod64("1.00", 0), &c) == 0);
    CHECK(tcc_d64_compare(tcc_strtod64("nan", 0), tcc_strtod64("1", 0), &c) == 2 && !c.flags);
    CHECK(tcc_d64_to_i64(tcc_strtod64("2.5", 0), &c) == 2 && c.flags == TCC_DEC_INEXACT);   /* ties to even */
    if (fails) { printf("decunit: %d failed\n", fails); return 1; }
    printf("decunit: ok\n");
    return 0;
}
