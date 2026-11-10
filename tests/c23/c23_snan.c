/* C23 <float.h> FLT_SNAN/DBL_SNAN/LDBL_SNAN and the __builtin_nan*,
   __builtin_nans*, __builtin_inf* constants.  Only objects with static
   initializers are inspected (by memcpy): loading an sNaN into an x87
   register would quiet it.  Prints "snan ok" and exits with 0. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <float.h>

#if __STDC_VERSION_FLOAT_H__ != 202311L
#error "__STDC_VERSION_FLOAT_H__"
#endif

static int err;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); err = 1; } } while (0)

static const float fs = FLT_SNAN, fq = NAN, fi = __builtin_inff();
static const double ds = DBL_SNAN, dq = __builtin_nan(""), di = __builtin_inf();
static const long double ls = LDBL_SNAN, lq = __builtin_nanl(""), li = __builtin_infl();
static struct { int a; double d; float f; } st = { 1, DBL_SNAN, FLT_SNAN };
static double arr[] = { __builtin_nans(""), 1.0, __builtin_nans("0") };

static uint32_t fb(const float *p) { uint32_t u; memcpy(&u, p, 4); return u; }
static uint64_t db(const double *p) { uint64_t u; memcpy(&u, p, 8); return u; }

/* long double, as double or x87 extended: 0 sNaN, 1 qNaN, 2 inf, -1 other */
static int ld_kind(const long double *p)
{
    if (sizeof(long double) == 8) {
        uint64_t u = db((const double *)p);
        if ((u & 0x7ff0000000000000ull) != 0x7ff0000000000000ull) return -1;
        if (!(u & 0x000fffffffffffffull)) return 2;
        return (u & 0x0008000000000000ull) != 0;
    } else {
        uint64_t m; uint16_t e;
        memcpy(&m, p, 8); memcpy(&e, (const char *)p + 8, 2);
        if ((e & 0x7fff) != 0x7fff) return -1;
        if (!(m << 1)) return 2;
        return (m >> 62 & 1) != 0;
    }
}

#define FNAN(u) (((u) & 0x7f800000u) == 0x7f800000u && ((u) & 0x007fffffu))
#define DNAN(u) (((u) & 0x7ff0000000000000ull) == 0x7ff0000000000000ull \
                 && ((u) & 0x000fffffffffffffull))

int main(void)
{
    _Static_assert(sizeof(FLT_SNAN) == sizeof(float), "float");
    _Static_assert(sizeof(DBL_SNAN) == sizeof(double), "double");
    _Static_assert(sizeof(LDBL_SNAN) == sizeof(long double), "long double");
    _Static_assert(sizeof(__builtin_nanf("")) == sizeof(float), "nanf");
    _Static_assert(_Generic(__builtin_infl(), long double: 1, default: 0), "infl");

    CHECK(fb(&fs) == 0x7fa00000u);
    CHECK(FNAN(fb(&fs)) && !(fb(&fs) & 0x00400000u));
    CHECK(FNAN(fb(&fq)) && (fb(&fq) & 0x00400000u));
    CHECK(fb(&fi) == 0x7f800000u);

    CHECK(db(&ds) == 0x7ff4000000000000ull);
    CHECK(DNAN(db(&ds)) && !(db(&ds) & 0x0008000000000000ull));
    CHECK(DNAN(db(&dq)) && (db(&dq) & 0x0008000000000000ull));
    CHECK(db(&di) == 0x7ff0000000000000ull);

    CHECK(ld_kind(&ls) == 0);
    CHECK(ld_kind(&lq) == 1);
    CHECK(ld_kind(&li) == 2);

    CHECK(st.a == 1 && db(&st.d) == 0x7ff4000000000000ull
          && fb(&st.f) == 0x7fa00000u);
    CHECK(db(&arr[0]) == 0x7ff4000000000000ull && arr[1] == 1.0
          && db(&arr[2]) == 0x7ff4000000000000ull);

    if (!err)
        printf("snan ok\n");
    return err;
}
