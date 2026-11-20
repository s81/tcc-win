/* Test driver for the tcc-win decimal library (lib/decbid.c): replays the
   vector file made by gen_vectors.py and checks result bits and exception
   flags.  usage: dectest vectors.txt [maxfail]
   Build: tcc -I include tests/decimal/dectest.c -o dectest.exe
   (libtcc1.a supplies the library).  Exit status 0 only if nothing failed.
   This file is part of tcc-win (LGPL-2.1, see COPYING). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <tcc_decimal.h>

typedef struct { uint64_t w[2]; } V;     /* a bit pattern of any width */

static V parse_hex(const char *s)
{
    V v = {{0, 0}}; int n = (int)strlen(s), i;
    for (i = 0; i < n; i++) {
        int d = s[i] <= '9' ? s[i] - '0' : s[i] - 'a' + 10;
        v.w[1] = (v.w[1] << 4) | (v.w[0] >> 60);
        v.w[0] = (v.w[0] << 4) | (uint64_t)d;
    }
    return v;
}
static int same(V a, V b) { return a.w[0] == b.w[0] && a.w[1] == b.w[1]; }
static void show(char *buf, int w, V v)
{
    if (w == 128) sprintf(buf, "%016llx%016llx", (unsigned long long)v.w[1], (unsigned long long)v.w[0]);
    else sprintf(buf, "%0*llx", w / 4, (unsigned long long)v.w[0]);
}
static int unhex(const char *h, char *out, int max)
{
    int n = 0;
    while (h[0] && h[1] && n < max) {
        int a = h[0] <= '9' ? h[0] - '0' : h[0] - 'a' + 10, b = h[1] <= '9' ? h[1] - '0' : h[1] - 'a' + 10;
        out[n++] = (char)(a * 16 + b); h += 2;
    }
    out[n] = 0; return n;
}

#define D32(v)  ((tcc_dec32){(uint32_t)(v).w[0]})
#define D64(v)  ((tcc_dec64){(v).w[0]})
#define D128(v) ((tcc_dec128){{(v).w[0], (v).w[1]}})
#define R32(x)  ((V){{(x).w, 0}})
#define R64(x)  ((V){{(x).w, 0}})
#define R128(x) ((V){{(x).w[0], (x).w[1]}})

/* switch over the format: X(P, D, R) with P = d32/d64/d128, D = unpack macro, R = pack macro */
#define BYW(w, X) ((w) == 32 ? X(d32, D32, R32) : (w) == 64 ? X(d64, D64, R64) : X(d128, D128, R128))

#define X_ADD(P, U, R) R(tcc_##P##_add(U(a), U(b), c))
#define X_SUB(P, U, R) R(tcc_##P##_sub(U(a), U(b), c))
#define X_MUL(P, U, R) R(tcc_##P##_mul(U(a), U(b), c))
#define X_DIV(P, U, R) R(tcc_##P##_div(U(a), U(b), c))
#define X_QUA(P, U, R) R(tcc_##P##_quantize(U(a), U(b), c))
#define X_SQRT(P, U, R) R(tcc_##P##_sqrt(U(a), c))
#define X_FMA(P, U, R) R(tcc_##P##_fma(U(a), U(b), U(d), c))
#define X_CMP(P, U, R) ((V){{(uint64_t)(int64_t)tcc_##P##_compare(U(a), U(b), c), 0}})
#define X_CVT(P, U, R) (g == 32 ? R32(tcc_##P##_to_d32(U(a), c)) : g == 64 ? R64(tcc_##P##_to_d64(U(a), c)) : R128(tcc_##P##_to_d128(U(a), c)))
#define X_FI64(P, U, R) R(tcc_##P##_from_i64((int64_t)a.w[0], c))
#define X_FU64(P, U, R) R(tcc_##P##_from_u64(a.w[0], c))
#define X_FDBL(P, U, R) R(tcc_##P##_from_double(bits2d(a.w[0]), c))
#define X_FFLT(P, U, R) R(tcc_##P##_from_float(bits2f((uint32_t)a.w[0]), c))
#define X_TI64(P, U, R) ((V){{(uint64_t)tcc_##P##_to_i64(U(a), c), 0}})
#define X_TU64(P, U, R) ((V){{tcc_##P##_to_u64(U(a), c), 0}})
#define X_TI32(P, U, R) ((V){{(uint32_t)tcc_##P##_to_i32(U(a), c), 0}})
#define X_TU32(P, U, R) ((V){{tcc_##P##_to_u32(U(a), c), 0}})
#define X_TDBL(P, U, R) ((V){{d2bits(tcc_##P##_to_double(U(a), c)), 0}})
#define X_TFLT(P, U, R) ((V){{f2bits(tcc_##P##_to_float(U(a), c)), 0}})
#define X_FSTR(P, U, R) R(tcc_##P##_from_string(str, endp, c))
#define X_TSTR(P, U, R) tcc_##P##_to_string(buf, sizeof buf, U(a), fmt)

static double bits2d(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
static float bits2f(uint32_t b) { float d; memcpy(&d, &b, 4); return d; }
static uint64_t d2bits(double d) { uint64_t b; memcpy(&b, &d, 8); return b; }
static uint32_t f2bits(float d) { uint32_t b; memcpy(&b, &d, 4); return b; }

static unsigned long ntot, nfail; static int maxfail = 20;
#define NOPS 32
static struct { char name[12]; int w; long pass, fail; } stats[NOPS * 3]; static int nstats;
static void count(const char *op, int w, int ok)
{
    int i;
    for (i = 0; i < nstats; i++) if (stats[i].w == w && !strcmp(stats[i].name, op)) break;
    if (i == nstats) { strcpy(stats[i].name, op); stats[i].w = w; nstats++; }
    ntot++;
    if (ok) stats[i].pass++; else { stats[i].fail++; nfail++; }
}

static V eval(const char *op, int w, int g, V a, V b, V d, tcc_dec_ctx *c, const char *str, char **endp)
{
    if (!strcmp(op, "add")) return BYW(w, X_ADD);
    if (!strcmp(op, "sub")) return BYW(w, X_SUB);
    if (!strcmp(op, "mul")) return BYW(w, X_MUL);
    if (!strcmp(op, "div")) return BYW(w, X_DIV);
    if (!strcmp(op, "quantize")) return BYW(w, X_QUA);
    if (!strcmp(op, "sqrt")) return BYW(w, X_SQRT);
    if (!strcmp(op, "fma")) return BYW(w, X_FMA);
    if (!strcmp(op, "compare")) return BYW(w, X_CMP);
    if (!strcmp(op, "cvt")) return BYW(w, X_CVT);
    if (!strcmp(op, "fromi64")) return BYW(w, X_FI64);
    if (!strcmp(op, "fromu64")) return BYW(w, X_FU64);
    if (!strcmp(op, "fromdbl")) return BYW(w, X_FDBL);
    if (!strcmp(op, "fromflt")) return BYW(w, X_FFLT);
    if (!strcmp(op, "toi64")) return BYW(w, X_TI64);
    if (!strcmp(op, "tou64")) return BYW(w, X_TU64);
    if (!strcmp(op, "toi32")) return BYW(w, X_TI32);
    if (!strcmp(op, "tou32")) return BYW(w, X_TU32);
    if (!strcmp(op, "todbl")) return BYW(w, X_TDBL);
    if (!strcmp(op, "toflt")) return BYW(w, X_TFLT);
    if (!strcmp(op, "fromstr")) return BYW(w, X_FSTR);
    fprintf(stderr, "unknown op %s\n", op); exit(2);
}

int main(int argc, char **argv)
{
    FILE *f; static char line[8192]; unsigned long lineno = 0; int i;
    if (argc < 2) { fprintf(stderr, "usage: dectest vectors.txt [maxfail]\n"); return 2; }
    if (argc > 2) maxfail = atoi(argv[2]);
    f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *tok[16]; int nt = 0, arrow = -1, w, ok, g = 0, r, ci;
        char *p = strtok(line, " \t\r\n"); char got[80], want[80], str[8192], buf[16384], out[16384];
        char *endp = 0; unsigned wantfl, gotfl; V a = {{0, 0}}, b = a, d = a, res, exp_; tcc_dec_ctx cx;
        lineno++;
        while (p && nt < 16) { tok[nt++] = p; p = strtok(NULL, " \t\r\n"); }
        for (i = 0; i < nt; i++) if (!strcmp(tok[i], "->")) arrow = i;
        if (nt < 4 || arrow < 0) continue;
        w = atoi(tok[1]);
        if (!strcmp(tok[0], "tostr")) {
            const char *fmt = tok[3][0] == 'f' ? "f" : "s"; int n;
            a = parse_hex(tok[2]);
            n = BYW(w, X_TSTR);
            unhex(tok[arrow + 1] + 1, out, sizeof out - 1);
            ok = (n == (int)strlen(out) && !strcmp(buf, out));
            if (!ok && nfail < (unsigned long)maxfail) printf("FAIL line %lu tostr %d: got len %d want len %d, got \"%.60s\" want \"%.60s\" tails \"%s\" \"%s\"\n", lineno, w, n, (int)strlen(out), buf, out, strlen(buf) > 30 ? buf + strlen(buf) - 30 : buf, strlen(out) > 30 ? out + strlen(out) - 30 : out);
            count("tostr", w, ok); continue;
        }
        r = atoi(tok[2]); cx.rounding = r; cx.flags = 0;
        wantfl = (unsigned)strtoul(tok[arrow + 2], 0, 16);
        if (!strcmp(tok[0], "fromstr")) { unhex(tok[3] + 1, str, sizeof str - 1); }
        else {
            a = parse_hex(tok[3]);
            if (arrow > 4 && strcmp(tok[0], "cvt")) b = parse_hex(tok[4]);
            if (arrow > 5) d = parse_hex(tok[5]);
            if (!strcmp(tok[0], "cvt")) g = atoi(tok[4]);
        }
        res = eval(tok[0], w, g, a, b, d, &cx, str, &endp);
        gotfl = cx.flags;
        if (!strcmp(tok[0], "compare")) { exp_ = (V){{(uint64_t)(int64_t)atoi(tok[arrow + 1]), 0}}; }
        else exp_ = parse_hex(tok[arrow + 1]);
        ok = same(res, exp_) && gotfl == wantfl;
        if (!strcmp(tok[0], "fromstr")) {
            ci = (int)(endp - str);
            if (ci != atoi(tok[arrow + 3])) ok = 0;
        }
        if (!ok && nfail < (unsigned long)maxfail) {
            int rw = !strcmp(tok[0], "cvt") ? g : (!strncmp(tok[0], "to", 2) || !strcmp(tok[0], "compare")) ? 64 : w;
            show(got, rw, res); show(want, rw, exp_);
            printf("FAIL line %lu %s w=%d r=%d in=%s%s%s%s%s: got %s fl=%x want %s fl=%x", lineno, tok[0], w, r,
                   !strcmp(tok[0], "fromstr") ? str : tok[3], arrow > 4 ? " " : "", arrow > 4 ? tok[4] : "",
                   arrow > 5 ? " " : "", arrow > 5 ? tok[5] : "", got, gotfl, want, wantfl);
            if (!strcmp(tok[0], "fromstr")) printf(" end=%d want %s", (int)(endp - str), tok[arrow + 3]);
            printf("\n");
        }
        count(tok[0], w, ok);
    }
    fclose(f);
    for (i = 0; i < nstats; i++) printf("%-9s %3d: pass %ld fail %ld\n", stats[i].name, stats[i].w, stats[i].pass, stats[i].fail);
    printf("dectest: %lu vectors, %lu failed\n", ntot, nfail);
    return nfail != 0;
}
