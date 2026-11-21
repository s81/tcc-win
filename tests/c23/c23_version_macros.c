/* C23 __STDC_VERSION_<header>_H__ */
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <time.h>

_Static_assert(__STDC_VERSION_FENV_H__ == 202311L, "fenv");
_Static_assert(__STDC_VERSION_FLOAT_H__ == 202311L, "float");
_Static_assert(__STDC_VERSION_INTTYPES_H__ == 202311L, "inttypes");
_Static_assert(__STDC_VERSION_SETJMP_H__ == 202311L, "setjmp");
_Static_assert(__STDC_VERSION_STDATOMIC_H__ == 202311L, "stdatomic");
_Static_assert(__STDC_VERSION_STDLIB_H__ == 202311L, "stdlib");
_Static_assert(__STDC_VERSION_STRING_H__ == 202311L, "string");
_Static_assert(__STDC_VERSION_TGMATH_H__ == 202311L, "tgmath");
_Static_assert(__STDC_VERSION_TIME_H__ == 202311L, "time");

int main(void) { return 0; }
