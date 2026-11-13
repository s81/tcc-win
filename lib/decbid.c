/* IEEE 754-2019 decimal floating point, BID encoding (part of libtcc1.a on
   Windows targets).  API: include/tcc_decimal.h.

   Clean-room: written from the IEEE 754-2019 format definitions and
   textbook integer arithmetic only.  No reciprocal approximations of powers
   of ten are used anywhere: every division by a power of ten is an exact
   integer division (schoolbook, 64-bit limbs; the 128/64 step is built from
   32-bit pieces because tcc has no __int128), and the power-of-ten tables
   hold exact integers.

   Layout of this file:
     1. integer primitives (limb arrays, little endian, u64 limbs)
     2. format descriptors, unpack and pack of the BID bit patterns
     3. rounding to a format (the single place where rounding, tininess,
        overflow and clamping are decided)
     4. + - * / sqrt fma quantize compare
     5. conversions: integers, binary floating point, strings
     6. the exported functions and the per-thread context

   Every finite number is held unpacked as sign, exponent and coefficient
   (c * 10^exp).  The wide coefficient has NL limbs (384 bits, enough for the
   exact result of a decimal128 fma); operations build the exact result (or
   the exact result plus a "sticky" mark for discarded nonzero digits) and
   round_to() rounds it once.  The sticky mark is only ever used together
   with at least p+2 digits, so it only decides ties.

   Choices where IEEE 754-2019 leaves freedom:
     - tininess is detected before rounding; Underflow = tiny and inexact;
     - a non-canonical encoding reads as zero (finite: coefficient above
       10^p-1 or exponent field above the maximum; NaN: payload above
       10^(p-1)-1), the exponent is kept (clamped);
     - NaN results propagate the first signaling operand (quieted), else the
       first quiet operand, with sign and payload; payloads that do not fit
       the destination format keep their low digits; invalid operations
       return the positive quiet NaN with payload 0;
     - quantize raises Inexact when it discards nonzero digits;
     - x/inf is zero with the smallest exponent.

   This file is part of tcc-win (LGPL-2.1, see COPYING). */

#include <stddef.h>
#include <stdatomic.h>
#include <tcc_decimal.h>

typedef unsigned long long u64;
#define M32 0xFFFFFFFFULL
#define NL 6                       /* limbs of the wide coefficient */
#define BNMAX 80                   /* limbs of the big conversion arrays */

/* ------------------------------------------------------------------ 1 */

static const u64 P10[20] = {
    1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL, 10000000ULL,
    100000000ULL, 1000000000ULL, 10000000000ULL, 100000000000ULL,
    1000000000000ULL, 10000000000000ULL, 100000000000000ULL,
    1000000000000000ULL, 10000000000000000ULL, 100000000000000000ULL,
    1000000000000000000ULL, 10000000000000000000ULL };

/* digits of 2^(b-1), b = 1..384 (exact table; index 0 unused) */
static const unsigned char DG[385] = {
    0,1,1,1,1,2,2,2,3,3,3,4,4,4,4,5,5,5,6,6,6,7,7,7,7,8,8,8,9,9,9,10,
    10,10,10,11,11,11,12,12,12,13,13,13,13,14,14,14,15,15,15,16,16,16,16,17,17,17,18,18,18,19,19,19,
    19,20,20,20,21,21,21,22,22,22,22,23,23,23,24,24,24,25,25,25,25,26,26,26,27,27,27,28,28,28,28,29,
    29,29,30,30,30,31,31,31,32,32,32,32,33,33,33,34,34,34,35,35,35,35,36,36,36,37,37,37,38,38,38,38,
    39,39,39,40,40,40,41,41,41,41,42,42,42,43,43,43,44,44,44,44,45,45,45,46,46,46,47,47,47,47,48,48,
    48,49,49,49,50,50,50,50,51,51,51,52,52,52,53,53,53,53,54,54,54,55,55,55,56,56,56,56,57,57,57,58,
    58,58,59,59,59,60,60,60,60,61,61,61,62,62,62,63,63,63,63,64,64,64,65,65,65,66,66,66,66,67,67,67,
    68,68,68,69,69,69,69,70,70,70,71,71,71,72,72,72,72,73,73,73,74,74,74,75,75,75,75,76,76,76,77,77,
    77,78,78,78,78,79,79,79,80,80,80,81,81,81,81,82,82,82,83,83,83,84,84,84,84,85,85,85,86,86,86,87,
    87,87,87,88,88,88,89,89,89,90,90,90,91,91,91,91,92,92,92,93,93,93,94,94,94,94,95,95,95,96,96,96,
    97,97,97,97,98,98,98,99,99,99,100,100,100,100,101,101,101,102,102,102,103,103,103,103,104,104,104,105,105,105,106,106,
    106,106,107,107,107,108,108,108,109,109,109,109,110,110,110,111,111,111,112,112,112,112,113,113,113,114,114,114,115,115,115,115,
    116 };

static int clz64(u64 x)             /* x != 0 */
{
    int n = 0;
    if (!(x >> 32)) { n += 32; x <<= 32; }
    if (!(x >> 48)) { n += 16; x <<= 16; }
    if (!(x >> 56)) { n += 8; x <<= 8; }
    if (!(x >> 60)) { n += 4; x <<= 4; }
    if (!(x >> 62)) { n += 2; x <<= 2; }
    if (!(x >> 63)) n++;
    return n;
}

static u64 mul64(u64 a, u64 b, u64 *hi)
{
    u64 a0 = a & M32, a1 = a >> 32, b0 = b & M32, b1 = b >> 32;
    u64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    u64 mid = (p00 >> 32) + (p01 & M32) + (p10 & M32);
    *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return (mid << 32) | (p00 & M32);
}

/* (hi:lo) / d with hi < d; 32-bit pieces (Hacker's Delight, divlu) */
static u64 div128(u64 hi, u64 lo, u64 d, u64 *rem)
{
    u64 dh, dl, un1, un0, q1, q0, rhat, un21;
    int s;
    if (!hi) { *rem = lo % d; return lo / d; }
    s = clz64(d);
    d <<= s;
    hi = (hi << s) | (s ? lo >> (64 - s) : 0);
    lo <<= s;
    dh = d >> 32; dl = d & M32;
    un1 = lo >> 32; un0 = lo & M32;
    q1 = hi / dh; rhat = hi - q1 * dh;
    while (q1 > M32 || q1 * dl > ((rhat << 32) | un1)) {
        q1--; rhat += dh;
        if (rhat > M32) break;
    }
    un21 = (hi << 32) + un1 - q1 * d;
    q0 = un21 / dh; rhat = un21 - q0 * dh;
    while (q0 > M32 || q0 * dl > ((rhat << 32) | un0)) {
        q0--; rhat += dh;
        if (rhat > M32) break;
    }
    *rem = ((un21 << 32) + un0 - q0 * d) >> s;
    return (q1 << 32) | q0;
}

static int bn_norm(const u64 *a, int n)
{
    while (n > 0 && !a[n - 1]) n--;
    return n;
}

static int bn_cmp(const u64 *a, int na, const u64 *b, int nb)
{
    int i;
    na = bn_norm(a, na); nb = bn_norm(b, nb);
    if (na != nb) return na < nb ? -1 : 1;
    for (i = na - 1; i >= 0; i--)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

static void bn_set(u64 *a, int n, u64 v)
{
    int i;
    a[0] = v;
    for (i = 1; i < n; i++) a[i] = 0;
}

/* a *= m over n limbs; returns the carry out */
static u64 bn_mul1(u64 *a, int n, u64 m)
{
    u64 carry = 0, hi, lo;
    int i;
    for (i = 0; i < n; i++) {
        lo = mul64(a[i], m, &hi);
        lo += carry; hi += lo < carry;
        a[i] = lo; carry = hi;
    }
    return carry;
}

/* a += v over n limbs; returns the carry out */
static u64 bn_add1(u64 *a, int n, u64 v)
{
    int i;
    for (i = 0; i < n && v; i++) { a[i] += v; v = a[i] < v; }
    return v;
}

static u64 bn_addn(u64 *a, const u64 *b, int n)
{
    u64 c = 0, t;
    int i;
    for (i = 0; i < n; i++) {
        t = a[i] + b[i]; { u64 c1 = t < a[i]; t += c; c = c1 | (t < c); }
        a[i] = t;
    }
    return c;
}

static u64 bn_subn(u64 *a, const u64 *b, int n)   /* a -= b */
{
    u64 br = 0, t;
    int i;
    for (i = 0; i < n; i++) {
        u64 b1 = a[i] < b[i];
        t = a[i] - b[i];
        b1 |= t < br; t -= br;
        a[i] = t; br = b1;
    }
    return br;
}

/* a /= d; returns the remainder */
static u64 bn_div1(u64 *a, int n, u64 d)
{
    u64 rem = 0;
    int i;
    for (i = n - 1; i >= 0; i--) a[i] = div128(rem, a[i], d, &rem);
    return rem;
}

/* r[na+nb] = a * b (no aliasing) */
static void bn_mul(u64 *r, const u64 *a, int na, const u64 *b, int nb)
{
    int i, j;
    for (i = 0; i < na + nb; i++) r[i] = 0;
    for (i = 0; i < na; i++) {
        u64 carry = 0, hi, lo;
        for (j = 0; j < nb; j++) {
            lo = mul64(a[i], b[j], &hi);
            lo += carry; hi += lo < carry;
            lo += r[i + j]; hi += lo < r[i + j];
            r[i + j] = lo; carry = hi;
        }
        r[i + nb] = carry;
    }
}

/* a <<= s bits (a has room: n limbs in, the result must fit n limbs) */
static void bn_shl(u64 *a, int n, int s)
{
    int w = s >> 6, b = s & 63, i;
    for (i = n - 1; i >= 0; i--) {
        u64 v = i - w >= 0 ? a[i - w] << b : 0;
        if (b && i - w - 1 >= 0) v |= a[i - w - 1] >> (64 - b);
        a[i] = v;
    }
}

static void bn_shr1(u64 *a, int n)
{
    int i;
    for (i = 0; i < n; i++) a[i] = (a[i] >> 1) | (i + 1 < n ? a[i + 1] << 63 : 0);
}

static int bn_bits(const u64 *a, int n)
{
    n = bn_norm(a, n);
    return n ? 64 * (n - 1) + 64 - clz64(a[n - 1]) : 0;
}

/* a *= 10^k; *n = limbs in use (grows); caller guarantees capacity */
static void bn_mulpow10(u64 *a, int *n, int k)
{
    while (k > 0) {
        int s = k > 19 ? 19 : k;
        u64 c = bn_mul1(a, *n, P10[s]);
        if (c) a[(*n)++] = c;
        k -= s;
    }
}

/* number of decimal digits of a nonzero NL-limb value */
static int bn_digits(const u64 *a, int n)
{
    u64 t[NL + 1]; int g, m, i, b;
    n = bn_norm(a, n);
    b = bn_bits(a, n);
    g = DG[b];
    if (n == 1) return a[0] >= P10[g] ? g + 1 : g;    /* g <= 19 here */
    for (i = 0; i < NL + 1; i++) t[i] = 0;
    t[0] = 1; m = 1;
    bn_mulpow10(t, &m, g);
    return bn_cmp(a, n, t, m) >= 0 ? g + 1 : g;
}

/* Knuth algorithm D. u (m limbs) / v (n limbs, top limb nonzero), m >= n.
   q gets m-n+1 limbs, r gets n limbs.  */
static void bn_divmod(u64 *q, u64 *r, const u64 *u, int m, const u64 *v, int n)
{
    u64 vn[BNMAX], un[BNMAX + 1];
    int s, i, j;
    if (n == 1) {
        for (i = 0; i < m; i++) q[i] = u[i];
        r[0] = bn_div1(q, m, v[0]);
        return;
    }
    s = clz64(v[n - 1]);
    for (i = n - 1; i > 0; i--) vn[i] = (v[i] << s) | (s ? v[i - 1] >> (64 - s) : 0);
    vn[0] = v[0] << s;
    un[m] = s ? u[m - 1] >> (64 - s) : 0;
    for (i = m - 1; i > 0; i--) un[i] = (u[i] << s) | (s ? u[i - 1] >> (64 - s) : 0);
    un[0] = u[0] << s;
    for (j = m - n; j >= 0; j--) {
        u64 qhat, rhat, borrow = 0, carry = 0, hi, lo, t;
        int rover = 0;
        if (un[j + n] >= vn[n - 1]) {
            qhat = ~0ULL; rhat = un[j + n - 1] + vn[n - 1];
            rover = rhat < vn[n - 1];
        } else
            qhat = div128(un[j + n], un[j + n - 1], vn[n - 1], &rhat);
        while (!rover) {
            lo = mul64(qhat, vn[n - 2], &hi);
            if (hi > rhat || (hi == rhat && lo > un[j + n - 2])) {
                qhat--; rhat += vn[n - 1];
                if (rhat < vn[n - 1]) rover = 1;
            } else
                break;
        }
        for (i = 0; i < n; i++) {
            u64 b1, b2;
            lo = mul64(qhat, vn[i], &hi);
            lo += carry; hi += lo < carry; carry = hi;
            b1 = un[i + j] < lo; t = un[i + j] - lo;
            b2 = t < borrow; t -= borrow;
            un[i + j] = t; borrow = b1 + b2;
        }
        {
            u64 b1 = un[j + n] < carry; t = un[j + n] - carry;
            u64 b2 = t < borrow; t -= borrow;
            un[j + n] = t;
            if (b1 | b2) {
                u64 c = 0;
                qhat--;
                for (i = 0; i < n; i++) {
                    u64 s1 = un[i + j] + vn[i], c1 = s1 < un[i + j], s2 = s1 + c;
                    c = c1 + (s2 < s1); un[i + j] = s2;
                }
                un[j + n] += c;
            }
        }
        q[j] = qhat;
    }
    for (i = 0; i < n; i++)
        r[i] = (un[i] >> s) | (s && i + 1 <= n ? un[i + 1] << (64 - s) : 0);
}

/* c >>= k decimal digits, exactly; returns 0: remainder 0, 1: below half,
   2: exactly half, 3: above half.  k >= 1. */
static int shr10(u64 *c, int n, int k)
{
    int low = 0, cls;
    u64 rem, half;
    n = bn_norm(c, n);
    while (k > 19) {
        rem = bn_div1(c, n, P10[19]); low |= rem != 0;
        n = bn_norm(c, n); k -= 19;
    }
    rem = bn_div1(c, n, P10[k]);
    half = 5 * P10[k - 1];
    cls = rem == 0 ? 0 : rem < half ? 1 : rem == half ? 2 : 3;
    if (low) cls = cls == 0 ? 1 : cls == 2 ? 3 : cls;
    return cls;
}

/* ------------------------------------------------------------------ 2 */

typedef struct {
    int n, p, eb, t, bias, etiny, qmax;
    u64 lim[2];          /* 10^p */
    u64 lim1[2];         /* 10^(p-1) */
} fmt;

static const fmt F32  = { 32, 7, 8, 20, 101, -101, 90,
                          { 10000000ULL, 0 }, { 1000000ULL, 0 } };
static const fmt F64  = { 64, 16, 10, 50, 398, -398, 369,
                          { 10000000000000000ULL, 0 }, { 1000000000000000ULL, 0 } };
static const fmt F128 = { 128, 34, 14, 110, 6176, -6176, 6111,
                          { 4003012203950112768ULL, 542101086242752ULL },
                          { 4089650035136921600ULL, 54210108624275ULL } };

typedef struct { int kind, sign, exp; u64 c[NL]; } num;   /* kind: 0 finite, 1 inf, 2 qNaN, 3 sNaN */

static void clearc(u64 *c) { int i; for (i = 0; i < NL; i++) c[i] = 0; }
static int iszero(const num *x) { return x->kind == 0 && !x->c[0] && !x->c[1] && !x->c[2] && !x->c[3] && !x->c[4] && !x->c[5]; }
static int cmp2(const u64 *a, const u64 *b) { return a[1] != b[1] ? (a[1] < b[1] ? -1 : 1) : a[0] != b[0] ? (a[0] < b[0] ? -1 : 1) : 0; }

static u64 getbits(u64 lo, u64 hi, int pos, int len)
{
    u64 v;
    if (pos >= 64) v = hi >> (pos - 64);
    else if (pos == 0) v = lo;
    else v = (lo >> pos) | (hi << (64 - pos));
    return len >= 64 ? v : v & ((1ULL << len) - 1);
}

static void lowbits(u64 lo, u64 hi, int len, u64 *out)
{
    out[0] = len >= 64 ? lo : lo & ((1ULL << len) - 1);
    out[1] = len > 64 ? hi & ((1ULL << (len - 64)) - 1) : 0;
}

static void unpack(const fmt *f, const u64 *b, num *x)
{
    int n = f->n, t = f->t;
    u64 lo = b[0], hi = n == 128 ? b[1] : 0, top5;
    clearc(x->c);
    x->sign = (int)getbits(lo, hi, n - 1, 1);
    top5 = getbits(lo, hi, n - 6, 5);
    x->exp = 0;
    if (top5 >= 30) {
        if (top5 == 30) { x->kind = 1; return; }
        x->kind = getbits(lo, hi, n - 7, 1) ? 3 : 2;
        lowbits(lo, hi, t, x->c);
        if (cmp2(x->c, f->lim1) >= 0) x->c[0] = x->c[1] = 0;
        return;
    }
    x->kind = 0;
    {
        u64 e;
        if ((top5 >> 3) != 3) {
            e = getbits(lo, hi, t + 3, f->eb);
            lowbits(lo, hi, t + 3, x->c);
        } else {
            e = getbits(lo, hi, t + 1, f->eb);
            lowbits(lo, hi, t + 1, x->c);
            if (n == 128) x->c[0] = x->c[1] = 0;      /* 2^113 > 10^34: non-canonical */
            else x->c[0] |= 1ULL << (t + 3);
        }
        if (e > (u64)(f->qmax + f->bias)) e = (u64)(f->qmax + f->bias);
        x->exp = (int)e - f->bias;
        if (cmp2(x->c, f->lim) >= 0) x->c[0] = x->c[1] = 0;
    }
}

static void pack(const fmt *f, const num *x, u64 *b)
{
    int n = f->n, t = f->t;
    u64 v = 0, h = 0, e;
    if (x->kind == 1) {
        if (n == 128) h = 0x1EULL << 58; else v = 0x1EULL << (n - 6);
    } else if (x->kind >= 2) {
        if (n == 128) { v = x->c[0]; h = (0x1FULL << 58) | ((u64)(x->kind == 3) << 57) | x->c[1]; }
        else v = (0x1FULL << (n - 6)) | ((u64)(x->kind == 3) << (n - 7)) | x->c[0];
    } else {
        e = (u64)(x->exp + f->bias);
        if (n == 128) { v = x->c[0]; h = x->c[1] | (e << 49); }
        else if (x->c[0] < (1ULL << (t + 3))) v = (e << (t + 3)) | x->c[0];
        else v = (3ULL << (n - 3)) | (e << (t + 1)) | (x->c[0] & ((1ULL << (t + 1)) - 1));
    }
    if (n == 128) { h |= (u64)x->sign << 63; b[0] = v; b[1] = h; }
    else { b[0] = v | ((u64)x->sign << (n - 1)); b[1] = 0; }
}

/* ------------------------------------------------------------------ 3 */

static tcc_dec_ctx *ctxof(tcc_dec_ctx *c)
{
    return c ? c : tcc_dec_thread_ctx();
}

static void mkzero(const fmt *f, int sign, int e, num *out)
{
    out->kind = 0; out->sign = sign; clearc(out->c);
    out->exp = e < f->etiny ? f->etiny : e > f->qmax ? f->qmax : e;
}

static void mkinf(num *out, int sign) { out->kind = 1; out->sign = sign; out->exp = 0; clearc(out->c); }

static void mknan(tcc_dec_ctx *cx, num *out)          /* default NaN, invalid operation */
{
    out->kind = 2; out->sign = 0; out->exp = 0; clearc(out->c);
    cx->flags |= TCC_DEC_INVALID;
}

static int roundup(int mode, int sign, int odd, int cls)
{
    switch (mode) {
    case TCC_DEC_NEAREST_EVEN: return cls > 2 || (cls == 2 && odd);
    case TCC_DEC_NEAREST_AWAY: return cls >= 2;
    case TCC_DEC_TOWARD_ZERO: return 0;
    case TCC_DEC_UPWARD: return cls && !sign;
    default: return cls && sign;
    }
}

/* overflow: infinity (returns 1) or the largest finite number (0), by mode and sign */
static int ovf_inf(tcc_dec_ctx *cx, int sign)
{
    cx->flags |= TCC_DEC_OVERFLOW | TCC_DEC_INEXACT;
    switch (cx->rounding) {
    case TCC_DEC_NEAREST_EVEN: case TCC_DEC_NEAREST_AWAY: return 1;
    case TCC_DEC_TOWARD_ZERO: return 0;
    case TCC_DEC_UPWARD: return !sign;
    default: return sign;
    }
}

static void overflow(const fmt *f, tcc_dec_ctx *cx, int sign, num *out)
{
    if (ovf_inf(cx, sign)) { mkinf(out, sign); return; }
    out->kind = 0; out->sign = sign; out->exp = f->qmax; clearc(out->c);
    out->c[0] = f->lim[0] - 1; out->c[1] = f->lim[1];
}

/* Round the exact value (-1)^sign * c * 10^e (+ a positive amount below one
   unit of c when sticky; then c has at least p+2 digits) into the format. */
static void round_to(const fmt *f, tcc_dec_ctx *cx, int sign, int e, u64 *c, int sticky, num *out)
{
    int n = bn_norm(c, NL), nd, k, tiny, cls = 0;
    if (!n) { mkzero(f, sign, e, out); return; }
    nd = bn_digits(c, n);
    k = nd - f->p;
    if (k < 0) k = 0;
    if (f->etiny - e > k) k = f->etiny - e;
    tiny = e + nd < f->etiny + f->p;
    if (k > 0) {
        if (k > nd) { clearc(c); cls = 1; }
        else cls = shr10(c, NL, k);
        if (sticky) cls = cls == 0 ? 1 : cls == 2 ? 3 : cls;
        e += k;
        if (cls) {
            cx->flags |= TCC_DEC_INEXACT;
            if (tiny) cx->flags |= TCC_DEC_UNDERFLOW;
            if (roundup(cx->rounding, sign, (int)(c[0] & 1), cls)) {
                if (!++c[0]) c[1]++;
                if (c[0] == f->lim[0] && c[1] == f->lim[1]) {   /* 10^p: renormalize */
                    c[0] = f->lim1[0]; c[1] = f->lim1[1]; e++;
                }
            }
        }
    }
    if (e > f->qmax) {
        if (!c[0] && !c[1]) e = f->qmax;
        else {
            int nq = bn_digits(c, 2);
            if (nq + e - f->qmax <= f->p) {
                int m = 2;
                bn_mulpow10(c, &m, e - f->qmax); e = f->qmax;
            } else { overflow(f, cx, sign, out); return; }
        }
    }
    out->kind = 0; out->sign = sign; out->exp = e;
    out->c[0] = c[0]; out->c[1] = c[1];
    { int i; for (i = 2; i < NL; i++) out->c[i] = 0; }
}

/* ------------------------------------------------------------------ 4 */

/* the NaN rule: first signaling operand, else first quiet one */
static int nan_pick(tcc_dec_ctx *cx, num *out, const num *a, const num *b, const num *c)
{
    const num *v[3]; int i, k;
    v[0] = a; v[1] = b; v[2] = c;
    for (k = 3; k >= 2; k--)
        for (i = 0; i < 3; i++)
            if (v[i] && v[i]->kind == k) {
                *out = *v[i];
                if (k == 3) { out->kind = 2; cx->flags |= TCC_DEC_INVALID; }
                return 1;
            }
    return 0;
}

static int floordiv2(int e) { return e >= 0 ? e / 2 : -((1 - e) / 2); }

/* drop trailing zeros of c while the exponent stays <= ideal */
static void strip_zeros(u64 *c, int *e, int ideal)
{
    static const int J[6] = { 32, 16, 8, 4, 2, 1 };
    int i;
    for (i = 0; i < 6; i++) {
        int j = J[i];
        if (*e + j <= ideal) {
            u64 t[NL]; int k, n = NL, ok = 1, r = j, m;
            for (k = 0; k < NL; k++) t[k] = c[k];
            while (r > 0 && ok) {
                int s = r > 19 ? 19 : r;
                n = bn_norm(t, n);
                ok = bn_div1(t, n, P10[s]) == 0; r -= s;
            }
            if (ok) { for (m = 0; m < NL; m++) c[m] = t[m]; *e += j; }
        }
    }
}

/* x +- y for finite operands given as wide coefficients */
static void add_fin(const fmt *f, tcc_dec_ctx *cx, int sx, const u64 *cxx, int ex,
                    int sy, const u64 *cyy, int ey, num *out)
{
    u64 X[NL], Y[NL], *a, *b;
    int zx, zy, i, nx, ny, g, d, n, sa, sb, ea, eb;
    for (i = 0; i < NL; i++) { X[i] = cxx[i]; Y[i] = cyy[i]; }
    zx = !bn_norm(X, NL); zy = !bn_norm(Y, NL);
    if (zx && zy) {
        mkzero(f, sx == sy ? sx : cx->rounding == TCC_DEC_DOWNWARD, ex < ey ? ex : ey, out);
        return;
    }
    if (zx || zy) {
        u64 *c = zx ? Y : X; int s = zx ? sy : sx, e = zx ? ey : ex, ez = zx ? ex : ey;
        if (ez < e) {
            int pad = e - ez, room = f->p - bn_digits(c, NL);
            if (pad > room) pad = room;
            if (pad > 0) { n = NL; n = bn_norm(c, NL); bn_mulpow10(c, &n, pad); e -= pad; }
        }
        round_to(f, cx, s, e, c, 0, out);
        return;
    }
    if (ex >= ey) { a = X; b = Y; sa = sx; sb = sy; ea = ex; eb = ey; }
    else { a = Y; b = X; sa = sy; sb = sx; ea = ey; eb = ex; }
    nx = bn_digits(a, NL); ny = bn_digits(b, NL);
    g = f->p + 2 - nx; if (g < 0) g = 0;
    if (eb + ny <= ea - g) {                    /* b only decides rounding */
        n = bn_norm(a, NL); bn_mulpow10(a, &n, g);
        if (sa != sb) { u64 one[NL]; bn_set(one, NL, 1); bn_subn(a, one, NL); }
        round_to(f, cx, sa, ea - g, a, 1, out);
        return;
    }
    d = ea - eb;
    n = bn_norm(a, NL); bn_mulpow10(a, &n, d);
    if (sa == sb) { bn_addn(a, b, NL); round_to(f, cx, sa, eb, a, 0, out); return; }
    i = bn_cmp(a, NL, b, NL);
    if (i == 0) { mkzero(f, cx->rounding == TCC_DEC_DOWNWARD, eb, out); return; }
    if (i > 0) { bn_subn(a, b, NL); round_to(f, cx, sa, eb, a, 0, out); }
    else { bn_subn(b, a, NL); round_to(f, cx, sb, eb, b, 0, out); }
}

/* generic sum of two numbers of any kind; yneg flips the sign of y (not of a NaN) */
static void add_num(const fmt *f, tcc_dec_ctx *cx, const num *x, const num *y, int yneg, num *out)
{
    int ys = y->sign ^ yneg;
    if (nan_pick(cx, out, x, y, 0)) return;
    if (x->kind == 1 || y->kind == 1) {
        if (x->kind == 1 && y->kind == 1 && x->sign != ys) { mknan(cx, out); return; }
        mkinf(out, x->kind == 1 ? x->sign : ys);
        return;
    }
    add_fin(f, cx, x->sign, x->c, x->exp, ys, y->c, y->exp, out);
}

static void mul_num(const fmt *f, tcc_dec_ctx *cx, const num *x, const num *y, num *out)
{
    int s = x->sign ^ y->sign;
    if (nan_pick(cx, out, x, y, 0)) return;
    if (x->kind == 1 || y->kind == 1) {
        if (iszero(x) || iszero(y)) { mknan(cx, out); return; }
        mkinf(out, s); return;
    }
    {
        u64 c[NL];
        clearc(c);
        bn_mul(c, x->c, 2, y->c, 2);
        round_to(f, cx, s, x->exp + y->exp, c, 0, out);
    }
}

static void div_num(const fmt *f, tcc_dec_ctx *cx, const num *x, const num *y, num *out)
{
    int s = x->sign ^ y->sign, da, db, sh, e, ideal, n, nb;
    u64 N[NL], q[NL], r[NL], v[2];
    if (nan_pick(cx, out, x, y, 0)) return;
    if (x->kind == 1) {
        if (y->kind == 1) { mknan(cx, out); return; }
        mkinf(out, s); return;
    }
    if (y->kind == 1) { mkzero(f, s, f->etiny, out); return; }
    if (iszero(y)) {
        if (iszero(x)) { mknan(cx, out); return; }
        cx->flags |= TCC_DEC_DIVBYZERO; mkinf(out, s); return;
    }
    ideal = x->exp - y->exp;
    if (iszero(x)) { mkzero(f, s, ideal, out); return; }
    da = bn_digits(x->c, 2); db = bn_digits(y->c, 2);
    sh = f->p + 1 + db - da;
    clearc(N); N[0] = x->c[0]; N[1] = x->c[1];
    n = 2; bn_mulpow10(N, &n, sh);
    v[0] = y->c[0]; v[1] = y->c[1]; nb = bn_norm(v, 2);
    clearc(q); clearc(r);
    bn_divmod(q, r, N, n, v, nb);
    e = ideal - sh;
    if (bn_norm(r, nb)) { round_to(f, cx, s, e, q, 1, out); return; }
    strip_zeros(q, &e, ideal);
    round_to(f, cx, s, e, q, 0, out);
}

static void sqrt_num(const fmt *f, tcc_dec_ctx *cx, const num *x, num *out)
{
    u64 R[NL], res[NL], t[NL];
    int e, da, m, n, nb, pos, ideal;
    if (nan_pick(cx, out, x, 0, 0)) return;
    if (x->kind == 1) { if (x->sign) mknan(cx, out); else mkinf(out, 0); return; }
    if (iszero(x)) { mkzero(f, x->sign, floordiv2(x->exp), out); return; }
    if (x->sign) { mknan(cx, out); return; }
    e = x->exp; clearc(R); R[0] = x->c[0]; R[1] = x->c[1];
    n = 2;
    if (e & 1) { bn_mulpow10(R, &n, 1); e--; }
    ideal = e / 2;
    da = bn_digits(R, NL);
    m = (2 * (f->p + 2) - da + 1) / 2; if (m < 0) m = 0;
    bn_mulpow10(R, &n, 2 * m);
    clearc(res);
    nb = bn_bits(R, NL);
    for (pos = (nb - 1) & ~1; pos >= 0; pos -= 2) {
        int i;
        for (i = 0; i < NL; i++) t[i] = res[i];
        t[pos >> 6] |= 1ULL << (pos & 63);
        if (bn_cmp(R, NL, t, NL) >= 0) {
            bn_subn(R, t, NL); bn_shr1(res, NL); res[pos >> 6] |= 1ULL << (pos & 63);
        } else
            bn_shr1(res, NL);
    }
    e = ideal - m;
    if (bn_norm(R, NL)) { round_to(f, cx, 0, e, res, 1, out); return; }
    strip_zeros(res, &e, ideal);
    round_to(f, cx, 0, e, res, 0, out);
}

static void fma_num(const fmt *f, tcc_dec_ctx *cx, const num *x, const num *y, const num *z, num *out)
{
    num p;
    if (x->kind == 3 || y->kind == 3) { nan_pick(cx, out, x, y, 0); return; }
    if (x->kind == 2 || y->kind == 2) { nan_pick(cx, &p, x, y, 0); }
    else if (x->kind == 1 || y->kind == 1) {
        if (iszero(x) || iszero(y)) { mknan(cx, out); return; }
        mkinf(&p, x->sign ^ y->sign);
    } else {
        p.kind = 0; p.sign = x->sign ^ y->sign; p.exp = x->exp + y->exp;
        clearc(p.c); bn_mul(p.c, x->c, 2, y->c, 2);
    }
    if (nan_pick(cx, out, &p, z, 0)) return;
    if (p.kind == 1 || z->kind == 1) {
        if (p.kind == 1 && z->kind == 1 && p.sign != z->sign) { mknan(cx, out); return; }
        mkinf(out, p.kind == 1 ? p.sign : z->sign); return;
    }
    add_fin(f, cx, p.sign, p.c, p.exp, z->sign, z->c, z->exp, out);
}

static void quantize_num(const fmt *f, tcc_dec_ctx *cx, const num *x, const num *y, num *out)
{
    int t, k, nd, cls = 0;
    u64 c[NL];
    if (nan_pick(cx, out, x, y, 0)) return;
    if (x->kind == 1 || y->kind == 1) {
        if (x->kind == 1 && y->kind == 1) { *out = *x; return; }
        mknan(cx, out); return;
    }
    t = y->exp;
    if (iszero(x)) { mkzero(f, x->sign, t, out); return; }
    clearc(c); c[0] = x->c[0]; c[1] = x->c[1];
    nd = bn_digits(c, 2);
    if (x->exp >= t) {
        int n = 2;
        if (nd + x->exp - t > f->p) { mknan(cx, out); return; }
        bn_mulpow10(c, &n, x->exp - t);
    } else {
        k = t - x->exp;
        if (k > nd) { clearc(c); cls = 1; }
        else cls = shr10(c, NL, k);
        if (cls) {
            if (roundup(cx->rounding, x->sign, (int)(c[0] & 1), cls)) {
                if (!++c[0]) c[1]++;
                if (c[0] == f->lim[0] && c[1] == f->lim[1]) { mknan(cx, out); return; }
            }
            cx->flags |= TCC_DEC_INEXACT;
        }
    }
    out->kind = 0; out->sign = x->sign; out->exp = t; clearc(out->c);
    out->c[0] = c[0]; out->c[1] = c[1];
}

/* -1, 0, 1; 2 if unordered */
static int compare_num(tcc_dec_ctx *cx, const num *x, const num *y)
{
    int s, dx, dy, c;
    if (x->kind >= 2 || y->kind >= 2) {
        if (x->kind == 3 || y->kind == 3) cx->flags |= TCC_DEC_INVALID;
        return 2;
    }
    if (x->kind == 1 || y->kind == 1) {
        int vx = x->kind == 1 ? (x->sign ? -2 : 2) : 0, vy = y->kind == 1 ? (y->sign ? -2 : 2) : 0;
        return vx == vy ? 0 : vx < vy ? -1 : 1;
    }
    if (iszero(x) || iszero(y)) {
        if (iszero(x) && iszero(y)) return 0;
        s = iszero(x) ? y->sign : x->sign;
        return iszero(x) ? (s ? 1 : -1) : (s ? -1 : 1);
    }
    if (x->sign != y->sign) return x->sign ? -1 : 1;
    dx = bn_digits(x->c, 2) + x->exp; dy = bn_digits(y->c, 2) + y->exp;
    if (dx != dy) c = dx < dy ? -1 : 1;
    else {
        u64 a[NL], b[NL]; int n;
        clearc(a); clearc(b); a[0] = x->c[0]; a[1] = x->c[1]; b[0] = y->c[0]; b[1] = y->c[1];
        if (x->exp > y->exp) { n = 2; bn_mulpow10(a, &n, x->exp - y->exp); }
        else if (y->exp > x->exp) { n = 2; bn_mulpow10(b, &n, y->exp - x->exp); }
        c = bn_cmp(a, NL, b, NL);
    }
    return x->sign ? -c : c;
}

/* ------------------------------------------------------------------ 5 */

static void from_u64(const fmt *f, tcc_dec_ctx *cx, int sign, u64 v, num *out)
{
    u64 c[NL];
    clearc(c); c[0] = v;
    round_to(f, cx, sign, 0, c, 0, out);
}

/* x rounded to an integer by the context mode; returns 0 ok, 1 NaN, 2 out of 64-bit magnitude range */
static int to_mag(tcc_dec_ctx *cx, const num *x, u64 *mag)
{
    u64 c[NL]; int nd, n;
    *mag = 0;
    if (x->kind >= 2) return 1;
    if (x->kind == 1) return 2;
    if (iszero(x)) return 0;
    clearc(c); c[0] = x->c[0]; c[1] = x->c[1];
    nd = bn_digits(c, 2);
    if (x->exp >= 0) {
        if (nd + x->exp > 20) return 2;
        n = 2; bn_mulpow10(c, &n, x->exp);
        if (bn_norm(c, NL) > 1) return 2;
    } else {
        int k = -x->exp, cls;
        if (k > nd) { clearc(c); cls = 1; }
        else cls = shr10(c, NL, k);
        if (cls) {
            cx->flags |= TCC_DEC_INEXACT;
            if (roundup(cx->rounding, x->sign, (int)(c[0] & 1), cls) && !++c[0]) c[1]++;
        }
        if (bn_norm(c, NL) > 1) return 2;
    }
    *mag = c[0];
    return 0;
}

static int64_t to_i64(tcc_dec_ctx *cx, const num *x, int64_t lo, int64_t hi)   /* lo, hi: the type's range */
{
    unsigned saved = cx->flags;
    u64 m; int r = to_mag(cx, x, &m);
    if (r == 1) { cx->flags |= TCC_DEC_INVALID; return 0; }
    if (r == 2 || (!x->sign && m > (u64)hi) || (x->sign && m != 0 && m > (u64)0 - (u64)lo)) {
        cx->flags = saved | TCC_DEC_INVALID;           /* only Invalid for out of range */
        return x->sign ? lo : hi;
    }
    return x->sign ? (int64_t)((u64)0 - m) : (int64_t)m;
}

static uint64_t to_u64(tcc_dec_ctx *cx, const num *x, uint64_t hi)
{
    unsigned saved = cx->flags;
    u64 m; int r = to_mag(cx, x, &m);
    if (r == 1) { cx->flags |= TCC_DEC_INVALID; return 0; }
    if (r == 2 || m > hi || (x->sign && m != 0)) {
        cx->flags = saved | TCC_DEC_INVALID;
        return x->sign ? 0 : hi;
    }
    return m;
}

/* convert between formats (x unpacked in any format) */
static void cvt_num(const fmt *g, tcc_dec_ctx *cx, const num *x, num *out)
{
    u64 c[NL];
    if (x->kind >= 2) {
        *out = *x;
        if (x->kind == 3) { out->kind = 2; cx->flags |= TCC_DEC_INVALID; }
        if (cmp2(out->c, g->lim1) >= 0) {           /* keep the low digits that fit */
            u64 q[2]; q[0] = out->c[0]; q[1] = out->c[1];
            out->c[0] = bn_div1(q, 2, g->lim1[0]); out->c[1] = 0;
        }
        return;
    }
    if (x->kind == 1) { mkinf(out, x->sign); return; }
    clearc(c); c[0] = x->c[0]; c[1] = x->c[1];
    round_to(g, cx, x->sign, x->exp, c, 0, out);
}

/* --- binary floating point --- */

/* Turn the big integer a[0..n) (clobbered) into: wide c holding its leading
   digits (at least p+2 of them, or all), the power of ten e10 of c's unit,
   and a sticky flag for nonzero digits below. */
static void big_top(u64 *a, int n, u64 *c, int *e10, int *sticky)
{
    u64 chunk[BNMAX * 2 + 8]; int nc = 0, i, j;
    *sticky = 0; clearc(c);
    n = bn_norm(a, n);
    while (n) { chunk[nc++] = bn_div1(a, n, P10[19]); n = bn_norm(a, n); }
    i = nc - 1;
    c[0] = chunk[i];
    while (i > 0 && nc - i < 3) {
        i--;
        bn_mul1(c, NL, P10[19]);
        bn_add1(c, NL, chunk[i]);
    }
    *e10 = 19 * i;
    for (j = 0; j < i; j++) if (chunk[j]) { *sticky = 1; break; }
}

static void from_binary(const fmt *f, tcc_dec_ctx *cx, int neg, u64 m, int e2, num *out)
{
    u64 a[BNMAX], c[NL]; int n, e10, sticky, k;
    if (!m) { mkzero(f, neg, 0, out); return; }
    while (e2 < 0 && !(m & 1)) { m >>= 1; e2++; }      /* exact value in lowest terms, as for an integer ratio */
    bn_set(a, BNMAX, 0);
    a[0] = m; n = 1; e10 = 0;
    if (e2 >= 0) {
        int w = e2 >> 6;
        a[w + 1] = 0; n = w + 2;
        bn_shl(a, n, e2); n = bn_norm(a, n);
    } else {
        k = -e2; e10 = e2;
        while (k >= 27) { u64 cc = bn_mul1(a, n, 7450580596923828125ULL); if (cc) a[n++] = cc; k -= 27; }
        if (k) {
            u64 p5 = 1; while (k--) p5 *= 5;
            { u64 cc = bn_mul1(a, n, p5); if (cc) a[n++] = cc; }
        }
    }
    big_top(a, n, c, &k, &sticky);
    round_to(f, cx, neg, e10 + k, c, sticky, out);
}

static void from_double(const fmt *f, tcc_dec_ctx *cx, double d, num *out)
{
    union { double d; u64 u; } v;
    int neg, ex; u64 fr;
    v.d = d; neg = (int)(v.u >> 63); ex = (int)((v.u >> 52) & 0x7FF); fr = v.u & 0xFFFFFFFFFFFFFULL;
    if (ex == 0x7FF) {
        if (!fr) { mkinf(out, neg); return; }
        out->kind = 2; out->sign = neg; out->exp = 0; clearc(out->c);   /* payload dropped */
        if (!(fr >> 51)) cx->flags |= TCC_DEC_INVALID;
        return;
    }
    if (ex) { fr |= 1ULL << 52; ex -= 1075; } else ex = -1074;
    from_binary(f, cx, neg, fr, ex, out);
}

/* nearest binary value of x with `mant` significand bits and exponent range emin..emax;
   returns the bit pattern (sign included) of a double (dbl) or float */
static u64 to_binary(tcc_dec_ctx *cx, const num *x, int dbl)
{
    int mant = dbl ? 53 : 24, emin = dbl ? -1022 : -126, emax = dbl ? 1023 : 127;
    int ebits = dbl ? 11 : 8, tot = dbl ? 64 : 32;
    u64 sgn = (u64)x->sign << (tot - 1), infb = ((1ULL << ebits) - 1) << (mant - 1);
    u64 N[BNMAX], D[BNMAX], Q[BNMAX], R[BNMAX], tmp[BNMAX];
    int nn, nd, dn, lg, sh, L, k2, E, u, cls, tiny, odd, i, sticky, ndig, lim;
    u64 M;
    if (x->kind == 3) { cx->flags |= TCC_DEC_INVALID; }
    if (x->kind >= 2) return sgn | infb | (1ULL << (mant - 2)) | 0;   /* quiet NaN, payload dropped */
    if (x->kind == 1) return sgn | infb;
    if (iszero(x)) return sgn;
    ndig = bn_digits(x->c, 2);
    lim = dbl ? 330 : 50;
    if (x->exp + ndig > lim) return ovf_inf(cx, x->sign) ? sgn | infb : sgn | (infb - 1);
    if (x->exp + ndig < -(dbl ? 360 : 90)) {                   /* far below the smallest subnormal */
        cx->flags |= TCC_DEC_INEXACT | TCC_DEC_UNDERFLOW;
        if ((cx->rounding == TCC_DEC_UPWARD && !x->sign) || (cx->rounding == TCC_DEC_DOWNWARD && x->sign)) return sgn | 1;
        return sgn;
    }
    bn_set(N, BNMAX, 0); bn_set(D, BNMAX, 0);
    N[0] = x->c[0]; N[1] = x->c[1]; nn = 2; D[0] = 1; dn = 1;
    if (x->exp >= 0) bn_mulpow10(N, &nn, x->exp); else bn_mulpow10(D, &dn, -x->exp);
    nn = bn_norm(N, nn);
    lg = bn_bits(N, nn) - bn_bits(D, dn);
    sh = mant + 3 - lg;                      /* Q = floor(N 2^sh / D) has mant+2..mant+4 bits */
    if (sh >= 0) { nn += sh / 64 + 1; if (nn > BNMAX - 2) nn = BNMAX - 2; bn_shl(N, nn, sh); nn = bn_norm(N, nn); }
    else { dn += (-sh) / 64 + 1; bn_shl(D, dn, -sh); dn = bn_norm(D, dn); }
    bn_divmod(Q, R, N, nn, D, dn);          /* nn >= dn by the choice of sh */
    sticky = bn_norm(R, dn) != 0;
    L = bn_bits(Q, nn - dn + 1);
    E = L - 1 - sh;                          /* exponent of the leading bit */
    tiny = E < emin;
    u = (tiny ? emin : E) - (mant - 1);
    k2 = u + sh;                             /* low bits of Q to drop, >= 2 */
    if (k2 > L) { bn_set(Q, BNMAX, 0); cls = 1; }
    else {
        bn_set(tmp, BNMAX, 0);
        for (i = 0; i < nn - dn + 1; i++) tmp[i] = Q[i];
        /* classify the dropped bits */
        {
            int bit = k2 - 1; u64 low = 0; int j;
            int tb = (int)((tmp[bit >> 6] >> (bit & 63)) & 1);
            for (j = 0; j < bit; j++) if ((tmp[j >> 6] >> (j & 63)) & 1) { low = 1; break; }
            cls = tb ? (low ? 3 : 2) : (low ? 1 : 0);
        }
        /* Q >>= k2 */
        { int w = k2 >> 6, b = k2 & 63, qn = nn - dn + 1;
          for (i = 0; i < qn; i++) {
              u64 v = i + w < qn ? tmp[i + w] >> b : 0;
              if (b && i + w + 1 < qn) v |= tmp[i + w + 1] << (64 - b);
              Q[i] = v;
          } }
    }
    if (sticky) cls = cls == 0 ? 1 : cls == 2 ? 3 : cls;
    M = Q[0];
    odd = (int)(M & 1);
    if (cls) {
        cx->flags |= TCC_DEC_INEXACT;
        if (tiny) cx->flags |= TCC_DEC_UNDERFLOW;
        if (roundup(cx->rounding, x->sign, odd, cls)) M++;
    }
    if (M == (1ULL << mant)) { M >>= 1; u++; }
    if (M >= (1ULL << (mant - 1))) {
        int be = u + (mant - 1);                  /* exponent of the leading bit */
        if (be > emax) {
            return ovf_inf(cx, x->sign) ? sgn | infb : sgn | (infb - 1);
        }
        return sgn | ((u64)(be + (dbl ? 1023 : 127)) << (mant - 1)) | (M & ((1ULL << (mant - 1)) - 1));
    }
    return sgn | M;
}

/* --- strings --- */

static int lc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int dig(int c) { return c >= '0' && c <= '9'; }

static int match(const char *s, const char *w)
{
    while (*w) { if (lc((unsigned char)*s) != *w) return 0; s++; w++; }
    return 1;
}

static void from_string(const fmt *f, tcc_dec_ctx *cx, const char *s, char **end, num *out)
{
    const char *q = s, *start;
    char buf[40]; int nbuf = 0, seen = 0, dot = 0, sign = 0, sticky = 0, i;
    long e10 = 0;
    u64 c[NL];
    while (*q == ' ' || (*q >= 9 && *q <= 13)) q++;
    if (*q == '-' || *q == '+') { sign = *q == '-'; q++; }
    start = q;
    if (match(q, "inf")) {
        q += match(q, "infinity") ? 8 : 3;
        mkinf(out, sign); if (end) *end = (char *)q; return;
    }
    if (match(q, "nan") || match(q, "snan")) {
        int snan = lc((unsigned char)*q) == 's', nd, z, keep = f->p - 1;
        const char *a = q + (snan ? 4 : 3), *d0, *d1, *pd;
        if (*a == '(') {
            d0 = a + 1; d1 = d0; while (dig((unsigned char)*d1)) d1++;
            q = *d1 == ')' ? d1 + 1 : a;
            if (*d1 != ')') d1 = d0;
        } else { d0 = a; d1 = d0; while (dig((unsigned char)*d1)) d1++; q = d1; }
        nd = (int)(d1 - d0); pd = d0;
        if (nd > keep) { pd += nd - keep; nd = keep; }
        out->kind = snan ? 3 : 2; out->sign = sign; out->exp = 0; clearc(out->c);
        for (z = 0; z < nd; z++) {
            u64 t[2]; t[0] = out->c[0]; t[1] = out->c[1];
            bn_mul1(t, 2, 10); bn_add1(t, 2, (u64)(pd[z] - '0'));
            out->c[0] = t[0]; out->c[1] = t[1];
        }
        if (end) *end = (char *)q; return;
    }
    for (;; q++) {
        int ch = (unsigned char)*q;
        if (dig(ch)) {
            seen = 1;
            if (nbuf == 0 && ch == '0') { if (dot) e10--; }
            else if (nbuf < 38) { buf[nbuf++] = (char)ch; if (dot) e10--; }
            else { if (ch != '0') sticky = 1; if (!dot) e10++; }
        } else if (ch == '.' && !dot) dot = 1;
        else break;
    }
    if (!seen) { mkzero(f, 0, 0, out); if (end) *end = (char *)s; return; }
    if ((*q == 'e' || *q == 'E')) {
        const char *r = q + 1; int es = 0; long ev = 0;
        if (*r == '-' || *r == '+') { es = *r == '-'; r++; }
        if (dig((unsigned char)*r)) {
            while (dig((unsigned char)*r)) { if (ev < 400000000L) ev = ev * 10 + (*r - '0'); r++; }
            e10 += es ? -ev : ev; q = r;
        }
    }
    (void)start;
    clearc(c);
    for (i = 0; i < nbuf; ) {
        int len = nbuf - i > 19 ? 19 : nbuf - i, j; u64 v = 0;
        for (j = 0; j < len; j++) v = v * 10 + (u64)(buf[i + j] - '0');
        bn_mul1(c, NL, P10[len]); bn_add1(c, NL, v);
        i += len;
    }
    round_to(f, cx, sign, (int)e10, c, sticky, out);
    if (end) *end = (char *)q;
}

typedef struct { char *b; size_t n; int len; } emit;
static void em(emit *e, char c) { if ((size_t)e->len + 1 < e->n) e->b[e->len] = c; e->len++; }
static void emzeros(emit *e, int k) { while (k-- > 0) em(e, '0'); }

static int to_string(const fmt *f, const num *x, char *buf, size_t n, const char *fmtc)
{
    emit e; char d[48]; int nd = 0, i, ex, adj, plain = fmtc && fmtc[0] == 'f';
    e.b = buf; e.n = n; e.len = 0;
    (void)f;
    if (x->sign) em(&e, '-');
    if (x->kind == 1) { const char *w = "Infinity"; while (*w) em(&e, *w++); }
    else if (x->kind >= 2) {
        const char *w = x->kind == 3 ? "sNaN" : "NaN";
        u64 t[2]; char r[40]; int k = 0;
        while (*w) em(&e, *w++);
        t[0] = x->c[0]; t[1] = x->c[1];
        while (bn_norm(t, 2)) { r[k++] = (char)('0' + bn_div1(t, 2, 10)); }
        while (k) em(&e, r[--k]);
    } else {
        u64 t[2]; char r[48]; int k = 0;
        t[0] = x->c[0]; t[1] = x->c[1];
        while (bn_norm(t, 2)) { u64 rem = bn_div1(t, 2, P10[19]); int j; for (j = 0; j < 19; j++) { r[k++] = (char)('0' + rem % 10); rem /= 10; } }
        while (k > 1 && r[k - 1] == '0') k--;
        if (!k) r[k++] = '0';
        for (i = 0; i < k; i++) d[nd++] = r[k - 1 - i];
        ex = x->exp; adj = ex + nd - 1;
        if (plain && ex > 0) { if (d[0] == '0') em(&e, '0'); else { for (i = 0; i < nd; i++) em(&e, d[i]); emzeros(&e, ex); } }
        else if (ex <= 0 && (plain || adj >= -6)) {
            if (ex == 0) for (i = 0; i < nd; i++) em(&e, d[i]);
            else if (nd > -ex) { for (i = 0; i < nd + ex; i++) em(&e, d[i]); em(&e, '.'); for (; i < nd; i++) em(&e, d[i]); }
            else { em(&e, '0'); em(&e, '.'); emzeros(&e, -ex - nd); for (i = 0; i < nd; i++) em(&e, d[i]); }
        } else {
            char r2[16]; int k2 = 0, a = adj < 0 ? -adj : adj;
            em(&e, d[0]);
            if (nd > 1) { em(&e, '.'); for (i = 1; i < nd; i++) em(&e, d[i]); }
            em(&e, 'E'); em(&e, adj < 0 ? '-' : '+');
            if (!a) r2[k2++] = '0';
            while (a) { r2[k2++] = (char)('0' + a % 10); a /= 10; }
            while (k2) em(&e, r2[--k2]);
        }
    }
    if (n) buf[(size_t)e.len < n ? (size_t)e.len : n - 1] = 0;
    return e.len;
}

/* ------------------------------------------------------------------ 6 */

#ifdef _WIN32
__declspec(dllimport) unsigned long __stdcall TlsAlloc(void);
__declspec(dllimport) int __stdcall TlsFree(unsigned long);
__declspec(dllimport) void *__stdcall TlsGetValue(unsigned long);
__declspec(dllimport) int __stdcall TlsSetValue(unsigned long, void *);
__declspec(dllimport) void *__stdcall LocalAlloc(unsigned, size_t);

static _Atomic long tls_index = -1;       /* the only global mutable state (the TLS slot) */
static tcc_dec_ctx tls_fallback;          /* only if TlsAlloc/LocalAlloc fail */

tcc_dec_ctx *tcc_dec_thread_ctx(void)
{
    long i = tls_index, none = -1;
    tcc_dec_ctx *c;
    if (i == -1) {
        unsigned long n = TlsAlloc();
        if (n == 0xFFFFFFFFUL) return &tls_fallback;
        if (!atomic_compare_exchange_strong(&tls_index, &none, (long)n)) TlsFree(n);   /* lost the race */
        i = tls_index;
    }
    c = (tcc_dec_ctx *)TlsGetValue((unsigned long)i);
    if (!c) {
        c = (tcc_dec_ctx *)LocalAlloc(0x40, sizeof *c);        /* LPTR: zeroed = nearest even, no flags */
        if (!c) return &tls_fallback;
        TlsSetValue((unsigned long)i, c);
    }
    return c;
}
#else
tcc_dec_ctx *tcc_dec_thread_ctx(void) { static tcc_dec_ctx c; return &c; }
#endif

static tcc_dec32  st32 (const fmt *f, const num *x) { u64 b[2]; tcc_dec32 r; pack(f, x, b); r.w = (uint32_t)b[0]; return r; }
static tcc_dec64  st64 (const fmt *f, const num *x) { u64 b[2]; tcc_dec64 r; pack(f, x, b); r.w = b[0]; return r; }
static tcc_dec128 st128(const fmt *f, const num *x) { u64 b[2]; tcc_dec128 r; pack(f, x, b); r.w[0] = b[0]; r.w[1] = b[1]; return r; }
static void ld32 (tcc_dec32 x, num *u)  { u64 b[2]; b[0] = x.w; b[1] = 0; unpack(&F32, b, u); }
static void ld64 (tcc_dec64 x, num *u)  { u64 b[2]; b[0] = x.w; b[1] = 0; unpack(&F64, b, u); }
static void ld128(tcc_dec128 x, num *u) { u64 b[2]; b[0] = x.w[0]; b[1] = x.w[1]; unpack(&F128, b, u); }

#define DEFFMT(P, T, FM, LDF, STF) \
T tcc_##P##_add(T a, T b, tcc_dec_ctx *c) { num x, y, r; c = ctxof(c); LDF(a, &x); LDF(b, &y); add_num(&FM, c, &x, &y, 0, &r); return STF(&FM, &r); } \
T tcc_##P##_sub(T a, T b, tcc_dec_ctx *c) { num x, y, r; c = ctxof(c); LDF(a, &x); LDF(b, &y); add_num(&FM, c, &x, &y, 1, &r); return STF(&FM, &r); } \
T tcc_##P##_mul(T a, T b, tcc_dec_ctx *c) { num x, y, r; c = ctxof(c); LDF(a, &x); LDF(b, &y); mul_num(&FM, c, &x, &y, &r); return STF(&FM, &r); } \
T tcc_##P##_div(T a, T b, tcc_dec_ctx *c) { num x, y, r; c = ctxof(c); LDF(a, &x); LDF(b, &y); div_num(&FM, c, &x, &y, &r); return STF(&FM, &r); } \
T tcc_##P##_sqrt(T a, tcc_dec_ctx *c) { num x, r; c = ctxof(c); LDF(a, &x); sqrt_num(&FM, c, &x, &r); return STF(&FM, &r); } \
T tcc_##P##_fma(T a, T b, T d, tcc_dec_ctx *c) { num x, y, z, r; c = ctxof(c); LDF(a, &x); LDF(b, &y); LDF(d, &z); fma_num(&FM, c, &x, &y, &z, &r); return STF(&FM, &r); } \
T tcc_##P##_quantize(T a, T b, tcc_dec_ctx *c) { num x, y, r; c = ctxof(c); LDF(a, &x); LDF(b, &y); quantize_num(&FM, c, &x, &y, &r); return STF(&FM, &r); } \
int tcc_##P##_compare(T a, T b, tcc_dec_ctx *c) { num x, y; c = ctxof(c); LDF(a, &x); LDF(b, &y); return compare_num(c, &x, &y); } \
T tcc_##P##_from_i64(int64_t v, tcc_dec_ctx *c) { num r; c = ctxof(c); from_u64(&FM, c, v < 0, v < 0 ? (u64)0 - (u64)v : (u64)v, &r); return STF(&FM, &r); } \
T tcc_##P##_from_u64(uint64_t v, tcc_dec_ctx *c) { num r; c = ctxof(c); from_u64(&FM, c, 0, v, &r); return STF(&FM, &r); } \
T tcc_##P##_from_double(double v, tcc_dec_ctx *c) { num r; c = ctxof(c); from_double(&FM, c, v, &r); return STF(&FM, &r); } \
T tcc_##P##_from_float(float v, tcc_dec_ctx *c) { return tcc_##P##_from_double((double)v, c); } \
int64_t tcc_##P##_to_i64(T a, tcc_dec_ctx *c) { num x; c = ctxof(c); LDF(a, &x); return to_i64(c, &x, (int64_t)(-0x7FFFFFFFFFFFFFFFLL - 1), 0x7FFFFFFFFFFFFFFFLL); } \
uint64_t tcc_##P##_to_u64(T a, tcc_dec_ctx *c) { num x; c = ctxof(c); LDF(a, &x); return to_u64(c, &x, 0xFFFFFFFFFFFFFFFFULL); } \
int32_t tcc_##P##_to_i32(T a, tcc_dec_ctx *c) { num x; c = ctxof(c); LDF(a, &x); return (int32_t)to_i64(c, &x, -2147483647LL - 1, 2147483647LL); } \
uint32_t tcc_##P##_to_u32(T a, tcc_dec_ctx *c) { num x; c = ctxof(c); LDF(a, &x); return (uint32_t)to_u64(c, &x, 0xFFFFFFFFULL); } \
double tcc_##P##_to_double(T a, tcc_dec_ctx *c) { num x; union { double d; u64 u; } v; c = ctxof(c); LDF(a, &x); v.u = to_binary(c, &x, 1); return v.d; } \
float tcc_##P##_to_float(T a, tcc_dec_ctx *c) { num x; union { float d; unsigned u; } v; c = ctxof(c); LDF(a, &x); v.u = (unsigned)to_binary(c, &x, 0); return v.d; } \
tcc_dec32 tcc_##P##_to_d32(T a, tcc_dec_ctx *c) { num x, r; c = ctxof(c); LDF(a, &x); cvt_num(&F32, c, &x, &r); return st32(&F32, &r); } \
tcc_dec64 tcc_##P##_to_d64(T a, tcc_dec_ctx *c) { num x, r; c = ctxof(c); LDF(a, &x); cvt_num(&F64, c, &x, &r); return st64(&F64, &r); } \
tcc_dec128 tcc_##P##_to_d128(T a, tcc_dec_ctx *c) { num x, r; c = ctxof(c); LDF(a, &x); cvt_num(&F128, c, &x, &r); return st128(&F128, &r); } \
T tcc_##P##_from_string(const char *s, char **end, tcc_dec_ctx *c) { num r; c = ctxof(c); from_string(&FM, c, s, end, &r); return STF(&FM, &r); } \
int tcc_##P##_to_string(char *buf, size_t n, T a, const char *fm) { num x; LDF(a, &x); return to_string(&FM, &x, buf, n, fm); }

DEFFMT(d32,  tcc_dec32,  F32,  ld32,  st32)
DEFFMT(d64,  tcc_dec64,  F64,  ld64,  st64)
DEFFMT(d128, tcc_dec128, F128, ld128, st128)

tcc_dec32  tcc_strtod32 (const char *s, char **end) { return tcc_d32_from_string(s, end, 0); }
tcc_dec64  tcc_strtod64 (const char *s, char **end) { return tcc_d64_from_string(s, end, 0); }
tcc_dec128 tcc_strtod128(const char *s, char **end) { return tcc_d128_from_string(s, end, 0); }
