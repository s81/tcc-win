/* C23 %b/%B in the printf family (lib/c23printf.c wraps msvcrt's printf).
   Prints "printf_b ok" and exits with 0 on success. */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static int err;

static void check(int line, const char *want, int wantret, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (r != wantret || strcmp(buf, want) != 0) {
        printf("FAIL line %d: \"%s\" gave \"%s\" (%d), want \"%s\" (%d)\n",
               line, fmt, buf, r, want, wantret);
        err = 1;
    }
}

#define T(want, ...) check(__LINE__, want, (int)strlen(want), __VA_ARGS__)

int main(void)
{
    char buf[8];
    int r, n = 0;

    T("101", "%b", 5u);
    T("0b101", "%#b", 5u);
    T("0B101", "%#B", 5u);
    T("0", "%b", 0u);
    T("0", "%#b", 0u);
    T("", "%.0b", 0u);
    T("00000101", "%08b", 5u);
    T("0b000101", "%#08b", 5u);
    T("101   |", "%-6b|", 5u);
    T("   101", "%6b", 5u);
    T("   101", "%*b", 6, 5u);
    T("0101", "%.4b", 5u);
    T("1000000000000000000000000000000000000000000000000000000000000001",
      "%llb", 0x8000000000000001ull);
    T("11111111111111111111111111111111", "%b", 0xffffffffu);
    T("0", "%hhb", 255 + 1);
    T("11", "%hhb", 256 + 3);
    T("1111111111111111", "%hb", 0x1ffffu);
    T("1010", "%lb", 10ul);
    T("1010", "%zb", (size_t)10);
    T("1010", "%jb", (unsigned long long)10);
    T("1 x 101 1.50e+03", "%d %s %b %.2e", 1, "x", 5u, 1500.0);
    T("  3.1", "%5.1f", 3.14159);
    T("1.000000e-300", "%e", 1e-300);
    T("-1.500e+00", "%.3e", -1.5);
    T(" 1.5e+00|", "%8.1e|", 1.5);
    T("01.5e+00", "%08.1e", 1.5);
    T("1E+100", "%G", 1e100);
    T("1e-05", "%g", 1e-5);
    T("inf -INF", "%f %F", 1.0 / 0.0, -1.0 / 0.0);
    T("-42 ff 0X1F 017", "%d %x %#X %#o", -42, 255u, 31u, 15u);
    T("-1 18446744073709551615", "%lld %llu", -1ll, ~0ull);
    T("44 -1", "%hhd %hd", 300, 65535);
    T("ab   |  abc|ab", "%-5s|%5s|%.2s", "ab", "abc", "abcdef");
    T("x%", "%c%%", 'x');

    r = snprintf(buf, sizeof buf, "%b", 0x3ffu);
    if (r != 10 || strcmp(buf, "1111111")) {
        printf("FAIL truncation: %d \"%s\"\n", r, buf);
        err = 1;
    }
    r = snprintf(NULL, 0, "%#b%d", 5u, 42);
    if (r != 7) {
        printf("FAIL size query: %d\n", r);
        err = 1;
    }
    r = sprintf(buf, "%b%n", 6u, &n);
    if (r != 3 || n != 3 || strcmp(buf, "110")) {
        printf("FAIL sprintf/%%n: %d %d \"%s\"\n", r, n, buf);
        err = 1;
    }
    if (!err)
        printf("printf_b ok\n");
    return err;
}
