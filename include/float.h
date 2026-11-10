#ifndef _FLOAT_H_
#define _FLOAT_H_

#define FLT_RADIX 2

/* IEEE float */
#define FLT_MANT_DIG 24
#define FLT_DIG 6
#define FLT_ROUNDS 1
#define FLT_EPSILON 1.19209290e-07F
#define FLT_MIN_EXP (-125)
#define FLT_MIN 1.17549435e-38F
#define FLT_MIN_10_EXP (-37)
#define FLT_MAX_EXP 128
#define FLT_MAX 3.40282347e+38F
#define FLT_MAX_10_EXP 38

/* IEEE double */
#define DBL_MANT_DIG 53
#define DBL_DIG 15
#define DBL_EPSILON 2.2204460492503131e-16
#define DBL_MIN_EXP (-1021)
#define DBL_MIN 2.2250738585072014e-308
#define DBL_MIN_10_EXP (-307)
#define DBL_MAX_EXP 1024
#define DBL_MAX 1.7976931348623157e+308
#define DBL_MAX_10_EXP 308

/* horrible intel long double */
#if defined __i386__ || defined __x86_64__

#define LDBL_MANT_DIG 64
#define LDBL_DIG 18
#define LDBL_EPSILON 1.08420217248550443401e-19L
#define LDBL_MIN_EXP (-16381)
#define LDBL_MIN 3.36210314311209350626e-4932L
#define LDBL_MIN_10_EXP (-4931)
#define LDBL_MAX_EXP 16384
#define LDBL_MAX 1.18973149535723176502e+4932L
#define LDBL_MAX_10_EXP 4932
#define DECIMAL_DIG 21

#elif defined __aarch64__ || defined __riscv
/*
 * Use values from:
 * gcc -dM -E -xc /dev/null | grep LDBL | sed -e "s/__//g"
 */
#define LDBL_MANT_DIG 113
#define LDBL_DIG 33
#define LDBL_EPSILON 1.92592994438723585305597794258492732e-34L
#define LDBL_MIN_EXP (-16381)
#define LDBL_MIN 3.36210314311209350626267781732175260e-4932L
#define LDBL_MIN_10_EXP (-4931)
#define LDBL_MAX_EXP 16384
#define LDBL_MAX 1.18973149535723176508575932662800702e+4932L
#define LDBL_MAX_10_EXP 4932
#define DECIMAL_DIG 36

#else

/* same as IEEE double */
#define LDBL_MANT_DIG 53
#define LDBL_DIG 15
#define LDBL_EPSILON 2.2204460492503131e-16L
#define LDBL_MIN_EXP (-1021)
#define LDBL_MIN 2.2250738585072014e-308L
#define LDBL_MIN_10_EXP (-307)
#define LDBL_MAX_EXP 1024
#define LDBL_MAX 1.7976931348623157e+308L
#define LDBL_MAX_10_EXP 308
#define DECIMAL_DIG 17

#endif

#if __STDC_VERSION__ >= 202311L
/* C23 7.7 (FLT_SNAN, DBL_SNAN, LDBL_SNAN are not provided: tcc has no
   signaling NaN constant, so __STDC_VERSION_FLOAT_H__ is not defined) */
#define INFINITY (1.0F/0.0F)
#define NAN (0.0F/0.0F)
#define FLT_IS_IEC_60559 1
#define DBL_IS_IEC_60559 1
#define LDBL_IS_IEC_60559 1
#define FLT_NORM_MAX FLT_MAX
#define DBL_NORM_MAX DBL_MAX
#define LDBL_NORM_MAX LDBL_MAX
#endif

#endif /* _FLOAT_H_ */
