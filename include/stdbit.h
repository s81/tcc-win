#ifndef _STDBIT_H
#define _STDBIT_H

/* C23 bit and byte utilities (7.18) */

#define __STDC_VERSION_STDBIT_H__ 202311L

#define __STDC_ENDIAN_LITTLE__ 1234
#define __STDC_ENDIAN_BIG__ 4321
#define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_LITTLE__ /* all Windows targets */

/* generic helpers: value zero-extended to 64 bits, 'n' = width in bits */
static inline unsigned __stdc_pop(unsigned long long x)
{
    x = x - ((x >> 1) & 0x5555555555555555ull);
    x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0full;
    return (unsigned)((x * 0x0101010101010101ull) >> 56);
}
static inline unsigned long long __stdc_mask(unsigned n)
{
    return n >= 64 ? ~0ull : (1ull << n) - 1;
}
static inline unsigned __stdc_width(unsigned long long x)
{
    unsigned w = 0;
    while (x)
        ++w, x >>= 1;
    return w;
}
static inline unsigned __stdc_lz(unsigned long long x, unsigned n)
{
    return n - __stdc_width(x);
}
static inline unsigned __stdc_tz(unsigned long long x, unsigned n)
{
    return x ? __stdc_pop((x & (0 - x)) - 1) : n;
}
static inline unsigned __stdc_lo(unsigned long long x, unsigned n)
{
    return __stdc_lz(~x & __stdc_mask(n), n);
}
static inline unsigned __stdc_to(unsigned long long x, unsigned n)
{
    return __stdc_tz(~x & __stdc_mask(n), n);
}
static inline unsigned __stdc_flz(unsigned long long x, unsigned n)
{
    unsigned r = __stdc_lo(x, n);
    return r == n ? 0 : r + 1;
}
static inline unsigned __stdc_flo(unsigned long long x, unsigned n)
{
    return x ? __stdc_lz(x, n) + 1 : 0;
}
static inline unsigned __stdc_ftz(unsigned long long x, unsigned n)
{
    unsigned r = __stdc_to(x, n);
    return r == n ? 0 : r + 1;
}
static inline unsigned __stdc_fto(unsigned long long x, unsigned n)
{
    return x ? __stdc_tz(x, n) + 1 : 0;
}
static inline unsigned __stdc_cz(unsigned long long x, unsigned n)
{
    return n - __stdc_pop(x);
}
static inline unsigned __stdc_co(unsigned long long x, unsigned n)
{
    (void)n;
    return __stdc_pop(x);
}
static inline unsigned __stdc_bw(unsigned long long x, unsigned n)
{
    (void)n;
    return __stdc_width(x);
}
static inline unsigned long long __stdc_bf(unsigned long long x)
{
    return x ? 1ull << (__stdc_width(x) - 1) : 0;
}
static inline unsigned long long __stdc_bc(unsigned long long x, unsigned n)
{
    unsigned w = x > 1 ? __stdc_width(x - 1) : 0;
    return w >= n ? 0 : 1ull << w; /* 0 if not representable */
}

/* typed functions */
#define __STDC_BIT_FUNCS(sfx, T) \
static inline unsigned stdc_leading_zeros_##sfx(T x) { return __stdc_lz(x, sizeof(T) * 8); } \
static inline unsigned stdc_leading_ones_##sfx(T x) { return __stdc_lo(x, sizeof(T) * 8); } \
static inline unsigned stdc_trailing_zeros_##sfx(T x) { return __stdc_tz(x, sizeof(T) * 8); } \
static inline unsigned stdc_trailing_ones_##sfx(T x) { return __stdc_to(x, sizeof(T) * 8); } \
static inline unsigned stdc_first_leading_zero_##sfx(T x) { return __stdc_flz(x, sizeof(T) * 8); } \
static inline unsigned stdc_first_leading_one_##sfx(T x) { return __stdc_flo(x, sizeof(T) * 8); } \
static inline unsigned stdc_first_trailing_zero_##sfx(T x) { return __stdc_ftz(x, sizeof(T) * 8); } \
static inline unsigned stdc_first_trailing_one_##sfx(T x) { return __stdc_fto(x, sizeof(T) * 8); } \
static inline unsigned stdc_count_zeros_##sfx(T x) { return __stdc_cz(x, sizeof(T) * 8); } \
static inline unsigned stdc_count_ones_##sfx(T x) { return __stdc_co(x, sizeof(T) * 8); } \
static inline _Bool stdc_has_single_bit_##sfx(T x) { return __stdc_pop(x) == 1; } \
static inline unsigned stdc_bit_width_##sfx(T x) { return __stdc_bw(x, sizeof(T) * 8); } \
static inline T stdc_bit_floor_##sfx(T x) { return (T)__stdc_bf(x); } \
static inline T stdc_bit_ceil_##sfx(T x) { return (T)__stdc_bc(x, sizeof(T) * 8); }

__STDC_BIT_FUNCS(uc, unsigned char)
__STDC_BIT_FUNCS(us, unsigned short)
__STDC_BIT_FUNCS(ui, unsigned int)
__STDC_BIT_FUNCS(ul, unsigned long)
__STDC_BIT_FUNCS(ull, unsigned long long)
#undef __STDC_BIT_FUNCS

/* type-generic macros (for unsigned integer types) */
#define __stdc_u(x) ((unsigned long long)(x) & __stdc_mask(sizeof(x) * 8))
#define __stdc_n(x) ((unsigned)(sizeof(x) * 8))
#define stdc_leading_zeros(x) __stdc_lz(__stdc_u(x), __stdc_n(x))
#define stdc_leading_ones(x) __stdc_lo(__stdc_u(x), __stdc_n(x))
#define stdc_trailing_zeros(x) __stdc_tz(__stdc_u(x), __stdc_n(x))
#define stdc_trailing_ones(x) __stdc_to(__stdc_u(x), __stdc_n(x))
#define stdc_first_leading_zero(x) __stdc_flz(__stdc_u(x), __stdc_n(x))
#define stdc_first_leading_one(x) __stdc_flo(__stdc_u(x), __stdc_n(x))
#define stdc_first_trailing_zero(x) __stdc_ftz(__stdc_u(x), __stdc_n(x))
#define stdc_first_trailing_one(x) __stdc_fto(__stdc_u(x), __stdc_n(x))
#define stdc_count_zeros(x) __stdc_cz(__stdc_u(x), __stdc_n(x))
#define stdc_count_ones(x) __stdc_co(__stdc_u(x), __stdc_n(x))
#define stdc_has_single_bit(x) ((_Bool)(__stdc_pop(__stdc_u(x)) == 1))
#define stdc_bit_width(x) __stdc_bw(__stdc_u(x), __stdc_n(x))
#define stdc_bit_floor(x) ((__typeof__(x))__stdc_bf(__stdc_u(x)))
#define stdc_bit_ceil(x) ((__typeof__(x))__stdc_bc(__stdc_u(x), __stdc_n(x)))

#endif /* _STDBIT_H */
