/* tcc_decimal.h - IEEE 754-2019 decimal floating point (BID encoding) for
   tcc-win.  The implementation is lib/decbid.c, part of libtcc1.a, so the
   functions below are available to AOT and -run programs without extra
   link options.  Windows targets only.

   Formats: tcc_dec32 / tcc_dec64 / tcc_dec128 hold the BID bit pattern.
   Every operation takes a context (rounding mode, sticky exception flags);
   a NULL context means the per-thread default, tcc_dec_thread_ctx().
   All of + - * / sqrt fma and every conversion are correctly rounded in all
   five IEEE rounding modes and raise the five IEEE exception flags.
   Tininess is detected before rounding (Underflow = tiny and inexact).

   Names: tcc_d32_*, tcc_d64_*, tcc_d128_*; the lists below show d64.

   This file is part of tcc-win (LGPL-2.1, see COPYING). */
#ifndef TCC_DECIMAL_H
#define TCC_DECIMAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct { uint32_t w; }    tcc_dec32;
typedef struct { uint64_t w; }    tcc_dec64;
typedef struct { uint64_t w[2]; } tcc_dec128;   /* w[0] = low half */

enum { TCC_DEC_NEAREST_EVEN, TCC_DEC_NEAREST_AWAY, TCC_DEC_TOWARD_ZERO,
       TCC_DEC_UPWARD, TCC_DEC_DOWNWARD };
enum { TCC_DEC_INVALID = 1, TCC_DEC_DIVBYZERO = 2, TCC_DEC_OVERFLOW = 4,
       TCC_DEC_UNDERFLOW = 8, TCC_DEC_INEXACT = 16 };

typedef struct { int rounding; unsigned flags; } tcc_dec_ctx;

/* Per-thread default context (Win32 TLS; 8 bytes per thread, never freed). */
tcc_dec_ctx *tcc_dec_thread_ctx(void);

#define TCC_DEC_DECLARE(P, T) \
T tcc_##P##_add(T, T, tcc_dec_ctx *); \
T tcc_##P##_sub(T, T, tcc_dec_ctx *); \
T tcc_##P##_mul(T, T, tcc_dec_ctx *); \
T tcc_##P##_div(T, T, tcc_dec_ctx *); \
T tcc_##P##_sqrt(T, tcc_dec_ctx *); \
T tcc_##P##_fma(T, T, T, tcc_dec_ctx *); \
/* result has the exponent of the second operand; invalid if it needs more digits */ \
T tcc_##P##_quantize(T, T, tcc_dec_ctx *); \
/* -1, 0, 1, or 2 if unordered (a NaN); Invalid only for a signaling NaN */ \
int tcc_##P##_compare(T, T, tcc_dec_ctx *); \
T tcc_##P##_from_i64(int64_t, tcc_dec_ctx *); \
T tcc_##P##_from_u64(uint64_t, tcc_dec_ctx *); \
T tcc_##P##_from_double(double, tcc_dec_ctx *); \
T tcc_##P##_from_float(float, tcc_dec_ctx *); \
/* integer conversions round by the context mode; Inexact if a fraction is \
   lost; NaN gives 0, out of range (and infinities) saturate; both set Invalid */ \
int64_t  tcc_##P##_to_i64(T, tcc_dec_ctx *); \
uint64_t tcc_##P##_to_u64(T, tcc_dec_ctx *); \
int32_t  tcc_##P##_to_i32(T, tcc_dec_ctx *); \
uint32_t tcc_##P##_to_u32(T, tcc_dec_ctx *); \
double   tcc_##P##_to_double(T, tcc_dec_ctx *); \
float    tcc_##P##_to_float(T, tcc_dec_ctx *); \
/* format conversions between the three decimal formats */ \
tcc_dec32  tcc_##P##_to_d32 (T, tcc_dec_ctx *); \
tcc_dec64  tcc_##P##_to_d64 (T, tcc_dec_ctx *); \
tcc_dec128 tcc_##P##_to_d128(T, tcc_dec_ctx *); \
/* strtod-style: leading blanks, sign, digits[.digits][e[sign]digits], \
   inf, infinity, nan, nan(digits), snan, snan(digits); *end = first unused \
   char (the start if nothing parsed) */ \
T tcc_##P##_from_string(const char *, char **end, tcc_dec_ctx *); \
/* fmt NULL, "" or "s": scientific string, cohort preserving (1.20E+3, \
   0.00123, -Infinity, NaN12, sNaN); "f": plain digits without exponent. \
   Returns the length the full text needs (snprintf style). */ \
int tcc_##P##_to_string(char *buf, size_t n, T, const char *fmt);

TCC_DEC_DECLARE(d32,  tcc_dec32)
TCC_DEC_DECLARE(d64,  tcc_dec64)
TCC_DEC_DECLARE(d128, tcc_dec128)

/* C23 names */
tcc_dec32  tcc_strtod32 (const char *, char **);
tcc_dec64  tcc_strtod64 (const char *, char **);
tcc_dec128 tcc_strtod128(const char *, char **);

#endif
