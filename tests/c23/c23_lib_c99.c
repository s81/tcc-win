/* C99/C11/C23 library functions that msvcrt.dll lacks or gets wrong.
   Exits with the line number of the first failing check (0 = ok). */
#include <ctype.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uchar.h>
#include <wchar.h>
#include <wctype.h>

#define CHECK(e) do { if (!(e)) return __LINE__; } while (0)

static int once_calls;
static void once_fn(void) { once_calls++; }

static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(void)
{
    char buf[64];
    wchar_t wbuf[32], *wend;
    char *end;
    int i, j;

    /* <fenv.h> */
    CHECK(feclearexcept(FE_ALL_EXCEPT) == 0 && fetestexcept(FE_ALL_EXCEPT) == 0);
    CHECK(feraiseexcept(FE_INEXACT) == 0 && fetestexcept(FE_ALL_EXCEPT) == FE_INEXACT);
    {
        fexcept_t fl;
        fenv_t env;
        femode_t mode;
        CHECK(fegetexceptflag(&fl, FE_ALL_EXCEPT) == 0 && fetestexceptflag(&fl, FE_INEXACT));
        CHECK(fegetenv(&env) == 0 && feholdexcept(&env) == 0 && fetestexcept(FE_ALL_EXCEPT) == 0);
        CHECK(feupdateenv(&env) == 0 && fetestexcept(FE_INEXACT));
        CHECK(fegetmode(&mode) == 0 && fesetround(FE_UPWARD) == 0 && fegetround() == FE_UPWARD);
        CHECK(fesetmode(&mode) == 0 && fegetround() == FE_TONEAREST);
        CHECK(fesetmode(FE_DFL_MODE) == 0);
    }
    feclearexcept(FE_ALL_EXCEPT);

    /* integer parsing: C99 names, and the C23 binary prefix */
    CHECK(strtoimax("-12", &end, 10) == -12 && *end == 0);
    CHECK(strtoumax("0xff", &end, 0) == 255);
    CHECK(wcstoimax(L"77", &wend, 8) == 63 && wcstoumax(L"9", 0, 10) == 9);
    CHECK(wcstoll(L"-5", 0, 10) == -5 && wcstoull(L"18446744073709551615", 0, 10) == UINT64_MAX);
    CHECK(strtol("0b101", &end, 0) == 5 && *end == 0);
    CHECK(strtoll("0B11", 0, 2) == 3 && strtoull("0b1", 0, 0) == 1);
    CHECK(strtoimax("0b11", 0, 0) == 3 && wcstol(L"0b100", 0, 0) == 4);
    CHECK(strtoll("0b", &end, 0) == 0 && *end == 'b');
    CHECK(wcstof(L"1.5", 0) == 1.5f && wcstold(L"2.5", 0) == 2.5L);
    CHECK(imaxdiv(7, 2).quot == 3 && imaxdiv(7, 2).rem == 1);
    CHECK(lldiv(-7, 2).quot == -3 && lldiv(-7, 2).rem == -1);
    CHECK(isblank(' ') && isblank('\t') && !isblank('\n') && !isblank('a'));
    CHECK(iswblank(L' ') && iswblank(L'\t') && !iswblank(L'x'));

    /* C23 %b in the printf and scanf families, PRIb* */
    snprintf(buf, sizeof buf, "%b|%#B|%" PRIb32 "|%" PRIb64, 5, 5, 6u, UINT64_C(7));
    CHECK(strcmp(buf, "101|0B101|110|111") == 0);
    CHECK(sscanf("101 0b11 17", "%b %" SCNb32 " %i", &i, &j, &i) == 3 && j == 3 && i == 17);
    CHECK(sscanf("7 0x1f", "%hhd %x", buf, &i) == 2 && buf[0] == 7 && i == 31);
    {
        signed char c[2] = { 1, 2 };
        CHECK(sscanf("300", "%hhd", &c[0]) == 1 && c[1] == 2);
    }
    CHECK(swscanf(L"0b11 abc", L"%b %ls", &i, wbuf) == 2 && i == 3 && wcscmp(wbuf, L"abc") == 0);
    CHECK(swprintf(wbuf, 32, L"%b|%d", 5, 7) == 5 && wcscmp(wbuf, L"101|7") == 0);
    CHECK(swprintf(wbuf, 4, L"%ls", L"abcdef") < 0 && wcscmp(wbuf, L"abc") == 0);
    {
        wchar_t s[] = L"a,b;c", *ctx, *t;
        int n = 0;
        for (t = wcstok(s, L",;", &ctx); t; t = wcstok(0, L",;", &ctx))
            n++;
        CHECK(n == 3);
    }

    /* <uchar.h>: ASCII round trip, which every locale supports */
    {
        mbstate_t st;
        char16_t c16;
        char32_t c32;
        char8_t c8;
        char out[8];
        memset(&st, 0, sizeof st);
        CHECK(mbrtoc16(&c16, "A", 1, &st) == 1 && c16 == 'A');
        CHECK(mbrtoc32(&c32, "B", 1, &st) == 1 && c32 == 'B');
        CHECK(mbrtoc8(&c8, "C", 1, &st) == 1 && c8 == 'C');
        CHECK(c16rtomb(out, u'D', &st) == 1 && out[0] == 'D');
        CHECK(c32rtomb(out, U'E', &st) == 1 && out[0] == 'E');
        CHECK(c8rtomb(out, u8'F', &st) == 1 && out[0] == 'F');
    }

    /* <stdlib.h> */
    {
        once_flag f = ONCE_FLAG_INIT;
        call_once(&f, once_fn);
        call_once(&f, once_fn);
        CHECK(once_calls == 1);
        CHECK(at_quick_exit(once_fn) == 0);
    }
    {
        const int a[3] = { 1, 2, 3 };
        int key = 2;
        CHECK(_Generic(bsearch(&key, a, 3, sizeof a[0], cmp), const void *: 1, default: 0));
        CHECK(*(const int *)bsearch(&key, a, 3, sizeof a[0], cmp) == 2);
    }

    /* <string.h>, <wchar.h>: the result keeps the const of the argument */
    {
        const char *cs = "abc";
        char ms[] = "abc";
        const wchar_t *cw = L"abc";
        CHECK(_Generic(strchr(cs, 'b'), const char *: 1, default: 0));
        CHECK(_Generic(strchr(ms, 'b'), char *: 1, default: 0));
        CHECK(_Generic(strstr(cs, "b"), const char *: 1, default: 0));
        CHECK(_Generic(memchr(cs, 'b', 3), const void *: 1, default: 0));
        CHECK(_Generic(wcschr(cw, L'b'), const wchar_t *: 1, default: 0));
        CHECK(strchr(cs, 'c') == cs + 2 && strstr(ms, "bc") == ms + 1);
    }

    /* <time.h>, <float.h> */
    {
        struct timespec t1, t2;
        CHECK(timespec_get(&t1, TIME_MONOTONIC) == TIME_MONOTONIC);
        CHECK(timespec_get(&t2, TIME_MONOTONIC) == TIME_MONOTONIC);
        CHECK(t2.tv_sec > t1.tv_sec || (t2.tv_sec == t1.tv_sec && t2.tv_nsec >= t1.tv_nsec));
    }
    CHECK(FLT_TRUE_MIN > 0 && DBL_TRUE_MIN > 0 && LDBL_TRUE_MIN > 0);
    CHECK(DBL_DECIMAL_DIG == 17 && FLT_DECIMAL_DIG == 9);
#ifdef _WIN32
    CHECK(LDBL_MANT_DIG == 53 && LDBL_DIG == 15); /* long double is double */
#endif
    return 0;
}
