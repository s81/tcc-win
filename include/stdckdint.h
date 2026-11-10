#ifndef _STDCKDINT_H
#define _STDCKDINT_H

/* C23 checked integer arithmetic (7.20).
   bool ckd_add(type1 *result, type2 a, type3 b);  also ckd_sub, ckd_mul.
   The operands may be of any integer type up to 64 bits.  The exact
   mathematical result is computed as a 128 bit two's complement number
   (hi:lo) and then checked against the range of *result. */

#define __STDC_VERSION_STDCKDINT_H__ 202311L

typedef struct { unsigned long long lo; long long hi; } __ckd_int;

static inline __ckd_int __ckd_add(unsigned long long a, int an,
                                  unsigned long long b, int bn)
{
    __ckd_int r;
    r.lo = a + b;
    r.hi = (an ? -1 : 0) + (bn ? -1 : 0) + (r.lo < a);
    return r;
}

static inline __ckd_int __ckd_sub(unsigned long long a, int an,
                                  unsigned long long b, int bn)
{
    __ckd_int r;
    r.lo = a - b;
    r.hi = (an ? -1 : 0) - (bn ? -1 : 0) - (a < b);
    return r;
}

static inline __ckd_int __ckd_mul(unsigned long long a, int an,
                                  unsigned long long b, int bn)
{
    __ckd_int r;
    unsigned long long al, ah, bl, bh, p0, p1, p2, p3, mid, hi;
    /* multiply the magnitudes: 64 x 64 -> 128 */
    if (an)
        a = 0 - a;
    if (bn)
        b = 0 - b;
    al = a & 0xffffffffu, ah = a >> 32;
    bl = b & 0xffffffffu, bh = b >> 32;
    p0 = al * bl, p1 = al * bh, p2 = ah * bl, p3 = ah * bh;
    mid = (p0 >> 32) + (p1 & 0xffffffffu) + (p2 & 0xffffffffu);
    r.lo = (p0 & 0xffffffffu) | (mid << 32);
    hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
    if (an != bn) { /* negate the 128 bit product */
        r.lo = 0 - r.lo;
        hi = ~hi + (r.lo == 0);
    }
    r.hi = (long long)hi;
    return r;
}

/* does the 128 bit value fit into an integer of 'bits' bits ? */
static inline _Bool __ckd_ovf(__ckd_int r, int bits, int is_signed)
{
    if (!is_signed)
        return r.hi != 0 || (bits < 64 && (r.lo >> bits) != 0);
    if (r.hi == 0)
        return (r.lo >> (bits - 1)) != 0;
    if (r.hi == -1)
        return ((long long)r.lo >> (bits - 1)) != -1;
    return 1;
}

#define __ckd_op(op, r, a, b) __extension__ ({ \
    __typeof__(a) __ckd_a = (a); \
    __typeof__(b) __ckd_b = (b); \
    __typeof__(r) __ckd_p = (r); \
    __ckd_int __ckd_r = op((unsigned long long)__ckd_a, __ckd_a < 0, \
                           (unsigned long long)__ckd_b, __ckd_b < 0); \
    *__ckd_p = (__typeof__(*__ckd_p))__ckd_r.lo; \
    __ckd_ovf(__ckd_r, sizeof(*__ckd_p) * 8, \
              (__typeof__(*__ckd_p))-1 < 0); })

#define ckd_add(r, a, b) __ckd_op(__ckd_add, r, a, b)
#define ckd_sub(r, a, b) __ckd_op(__ckd_sub, r, a, b)
#define ckd_mul(r, a, b) __ckd_op(__ckd_mul, r, a, b)

#endif /* _STDCKDINT_H */
