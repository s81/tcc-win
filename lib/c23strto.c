/* Integer parsing for Windows targets (part of libtcc1.a):

   msvcrt.dll has no strtoimax/strtoumax/wcstoimax/wcstoumax/wcstoll/wcstoull
   and its strtol family does not know the C23 binary prefix ("0b101" with
   base 0 or 2).  Every function here is one engine over a narrow or wide
   string.  In C23 mode <stdlib.h>, <wchar.h> and <inttypes.h> map strtol,
   strtoul, strtoll, strtoull, wcstol, wcstoul, wcstoll, wcstoull, strtoimax,
   strtoumax, wcstoimax and wcstoumax to the __tcc_ versions that accept
   0b/0B (opt out with -D__TCC_MSVCRT_STRTO); the plain names keep the C17
   meaning.

   wcstof and wcstold (msvcrt has only wcstod) are here too.

   Without _WIN32 this builds against the host C library for unit tests
   (rename the public names with -D, see tests/c99rt-host.sh). */

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
__declspec(dllimport) int *_errno(void);
__declspec(dllimport) int isspace(int);
__declspec(dllimport) int iswspace(unsigned short);
#define SET_ERRNO(v) (*_errno() = (v))
#define IS_SPACE_W(c) iswspace(c)
#else
#include <errno.h>
#include <ctype.h>
#include <wctype.h>
#define SET_ERRNO(v) (errno = (v))
#define IS_SPACE_W(c) iswspace(c)
#endif

#ifdef _WIN32
__declspec(dllimport) double wcstod(const wchar_t *, wchar_t **);
#endif

#define EINVAL_ 22
#define ERANGE_ 34

#define CH(i) (wide ? (unsigned)((const wchar_t *)s)[i] : (unsigned)((const unsigned char *)s)[i])

static int digit_of(unsigned c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 99;
}

/* Parses the magnitude.  Returns it in *mag, sets *neg, *ovf (the value did
   not fit 64 bits) and *end (the end of the parsed text, or nptr if no
   conversion could be performed). */
static void parse(const void *nptr, size_t *end, int base, int wide, int allow_b,
                  unsigned long long *mag, int *neg, int *ovf)
{
    const void *s = nptr;
    size_t i = 0;
    int d, any = 0;
    unsigned long long acc = 0;

    *mag = 0, *neg = 0, *ovf = 0, *end = 0;
    if (base < 0 || base == 1 || base > 36) {
        SET_ERRNO(EINVAL_);
        return;
    }
    for (;; i++) {
        unsigned c = CH(i);
        if (!(wide ? IS_SPACE_W(c) : (c < 256 && isspace(c))))
            break;
    }
    if (CH(i) == '-' || CH(i) == '+')
        *neg = CH(i++) == '-';
    if ((base == 0 || base == 16) && CH(i) == '0' && (CH(i + 1) | 32) == 'x'
        && digit_of(CH(i + 2)) < 16) {
        base = 16, i += 2;
    } else if (allow_b && (base == 0 || base == 2) && CH(i) == '0'
               && (CH(i + 1) | 32) == 'b' && (CH(i + 2) == '0' || CH(i + 2) == '1')) {
        base = 2, i += 2;
    } else if (base == 0) {
        base = CH(i) == '0' ? 8 : 10;
    }
    while ((d = digit_of(CH(i))) < base) {
        any = 1;
        if (acc > (~0ULL - d) / base)
            *ovf = 1;
        else
            acc = acc * base + d;
        i++;
    }
    if (any)
        *end = i;
    *mag = acc;
}

static long long to_signed(unsigned long long mag, int neg, int ovf, long long max)
{
    if (!ovf && !neg && mag <= (unsigned long long)max)
        return (long long)mag;
    if (!ovf && neg && mag <= (unsigned long long)max + 1)
        return (long long)(0 - mag);
    SET_ERRNO(ERANGE_);
    return neg ? -max - 1 : max;
}

static unsigned long long to_unsigned(unsigned long long mag, int neg, int ovf,
                                      unsigned long long max)
{
    if (ovf || mag > max) {
        SET_ERRNO(ERANGE_);
        return max;
    }
    return neg ? (0 - mag) & max : mag;
}

#define LONG_MAX_ ((long long)(((unsigned long)-1) >> 1))
#define ULONG_MAX_ ((unsigned long long)(unsigned long)-1)

static long long conv_s(const void *s, void **e, int base, int wide, int allow_b,
                        long long max)
{
    unsigned long long mag;
    int neg, ovf;
    size_t end;
    parse(s, &end, base, wide, allow_b, &mag, &neg, &ovf);
    if (e)
        *e = wide ? (void *)((wchar_t *)s + end) : (void *)((char *)s + end);
    return to_signed(mag, neg, ovf, max);
}

static unsigned long long conv_u(const void *s, void **e, int base, int wide,
                                 int allow_b, unsigned long long max)
{
    unsigned long long mag;
    int neg, ovf;
    size_t end;
    parse(s, &end, base, wide, allow_b, &mag, &neg, &ovf);
    if (e)
        *e = wide ? (void *)((wchar_t *)s + end) : (void *)((char *)s + end);
    return to_unsigned(mag, neg, ovf, max);
}

#define N(s, e, b) (s), (void **)(e), (b)

/* C23: binary prefix accepted */
long __tcc_strtol(const char *s, char **e, int b)
{ return (long)conv_s(N(s, e, b), 0, 1, LONG_MAX_); }
unsigned long __tcc_strtoul(const char *s, char **e, int b)
{ return (unsigned long)conv_u(N(s, e, b), 0, 1, ULONG_MAX_); }
long long __tcc_strtoll(const char *s, char **e, int b)
{ return conv_s(N(s, e, b), 0, 1, 0x7fffffffffffffffLL); }
unsigned long long __tcc_strtoull(const char *s, char **e, int b)
{ return conv_u(N(s, e, b), 0, 1, ~0ULL); }
long __tcc_wcstol(const wchar_t *s, wchar_t **e, int b)
{ return (long)conv_s(N(s, e, b), 1, 1, LONG_MAX_); }
unsigned long __tcc_wcstoul(const wchar_t *s, wchar_t **e, int b)
{ return (unsigned long)conv_u(N(s, e, b), 1, 1, ULONG_MAX_); }
long long __tcc_wcstoll(const wchar_t *s, wchar_t **e, int b)
{ return conv_s(N(s, e, b), 1, 1, 0x7fffffffffffffffLL); }
unsigned long long __tcc_wcstoull(const wchar_t *s, wchar_t **e, int b)
{ return conv_u(N(s, e, b), 1, 1, ~0ULL); }

/* C99 7.8.2 and 7.24.4.1.2: msvcrt has none of these (the strto*i64 and
   wcsto*i64 names do the same job but are not the standard names) */
intmax_t strtoimax(const char *s, char **e, int b)
{ return conv_s(N(s, e, b), 0, 0, 0x7fffffffffffffffLL); }
uintmax_t strtoumax(const char *s, char **e, int b)
{ return conv_u(N(s, e, b), 0, 0, ~0ULL); }
intmax_t wcstoimax(const wchar_t *s, wchar_t **e, int b)
{ return conv_s(N(s, e, b), 1, 0, 0x7fffffffffffffffLL); }
uintmax_t wcstoumax(const wchar_t *s, wchar_t **e, int b)
{ return conv_u(N(s, e, b), 1, 0, ~0ULL); }
long long wcstoll(const wchar_t *s, wchar_t **e, int b)
{ return conv_s(N(s, e, b), 1, 0, 0x7fffffffffffffffLL); }
unsigned long long wcstoull(const wchar_t *s, wchar_t **e, int b)
{ return conv_u(N(s, e, b), 1, 0, ~0ULL); }

/* C99 7.24.4.1.1: wcstof and wcstold from msvcrt's wcstod, as <stdlib.h>
   does for strtof and strtold (long double is double here).  wcstof
   rounds twice (decimal to double to float). */
float wcstof(const wchar_t *s, wchar_t **e)
{
    return (float)wcstod(s, e);
}

long double wcstold(const wchar_t *s, wchar_t **e)
{
    return wcstod(s, e);
}
