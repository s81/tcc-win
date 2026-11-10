/* More C23 features of tcc-win. Exits with 0 on success. */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <limits.h>

static int err;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); err = 1; } } while (0)

/* __VA_OPT__ */
#define COUNT(...) count(0 __VA_OPT__(,) __VA_ARGS__, -1)
#define SEL(a, ...) __VA_OPT__(1 +) a
#define CAT(a, ...) a ## __VA_OPT__(1) ## 2
#define STR_(...) #__VA_ARGS__
#define STR(...) STR_(__VA_ARGS__)
#define OPT(...) [__VA_OPT__(x __VA_ARGS__ y)]

static int count(int first, ...)
{
    va_list ap;
    int n = 0;
    va_start(ap, first);                  /* classic form */
    while (va_arg(ap, int) != -1)
        n++;
    va_end(ap);
    return n;
}

/* va_start with one argument */
static int sum(int n, ...)
{
    va_list ap;
    int s = 0;
    va_start(ap);
    while (n--)
        s += va_arg(ap, int);
    va_end(ap);
    return s;
}

/* variadic function without named parameter */
static double mixed(...)
{
    va_list ap;
    va_start(ap);
    int a = va_arg(ap, int);
    double b = va_arg(ap, double);
    long long c = va_arg(ap, long long);
    const char *s = va_arg(ap, const char *);
    int e = va_arg(ap, int);
    int f = va_arg(ap, int);
    va_end(ap);
    return a + b + c + strlen(s) + e + f;
}

struct big { long long a, b, c; };
static struct big bigret(...)
{
    struct big r;
    va_list ap;
    va_start(ap);
    r.a = va_arg(ap, int);
    r.b = va_arg(ap, int);
    r.c = 0;
    va_end(ap);
    return r;
}

#include <uchar.h>
#define STRV(...) #__VA_OPT__(a __VA_ARGS__ b)

/* attributes in all positions */
struct [[deprecated]] attr_s { int a [[maybe_unused]]; };
enum attr_e { ATTR_A [[deprecated]] = 4, ATTR_B };
static int attr_g [[maybe_unused]] = 1;
static int attr_f(int x [[maybe_unused]]) [[deprecated]];
static int attr_f(int x) { return x + ATTR_B + attr_g; }
static const char8_t *u8s = u8"h\u00e9";

static int noargs() { return 3; }           /* '()' is '(void)' in C23 */

constexpr double PI = 3.25;
constexpr float HALF = 0.5f;
static double tau = 2 * PI;                  /* usable in constant expressions */
static const unsigned char part[] = {
#embed "embed.txt" limit(3) prefix('<',) suffix(,'>') if_empty(0)
};
static const unsigned char none[] = {
#embed "embed.txt" limit(0) if_empty('e', 'm')
};
#if __has_embed("embed.txt" limit(0)) != __STDC_EMBED_EMPTY__
# error "__has_embed with limit(0)"
#endif

int main(void)
{
    CHECK(COUNT() == 0 && COUNT(1) == 1 && COUNT(1, 2, 3) == 3);
    CHECK(SEL(5) == 5 && SEL(5, x) == 6);
    CHECK(CAT(4) == 42 && CAT(4, x) == 412);
    CHECK(!strcmp(STR(OPT()), "[]") && !strcmp(STR(OPT(1)), "[x 1 y]"));

    CHECK(sum(3, 1, 2, 3) == 6 && sum(0) == 0);
    CHECK(mixed(1, 2.5, 3LL, "four", 5, 6) == 21.5);
    struct big b = bigret(7, 8);
    CHECK(b.a == 7 && b.b == 8);
    CHECK(noargs() == 3);
    CHECK(attr_f(1) == 7 && sizeof(struct attr_s) == sizeof(int));
    CHECK(strlen((const char *)u8s) == 3 && sizeof(u8"ab") == 3);
    CHECK(!strcmp(STRV(1, "q"), "a 1, \"q\" b") && !strcmp(STRV(), ""));

    CHECK(tau == 6.5 && HALF * 4 == 2 && sizeof(HALF) == sizeof(float));
    CHECK(sizeof part == 5 && !memcmp(part, "<emb>", 5));
    CHECK(sizeof none == 2 && none[0] == 'e');

    /* nullptr_t is a distinct type */
    CHECK(_Generic(nullptr, nullptr_t: 1, void *: 2, default: 3) == 1);
    CHECK(_Generic((void *)0, nullptr_t: 1, void *: 2, default: 3) == 2);
    nullptr_t np = nullptr;
    int *ip = np;
    CHECK(ip == nullptr && !np);

    /* storage class specifiers in compound literals */
    int *sp = (static int[]){ 1, 2, 3 };
    CHECK(sp[2] == 3 && (constexpr int){ 9 } == 9);

    /* <stdbit.h> */
    CHECK(stdc_leading_zeros((unsigned char)1) == 7 && stdc_leading_zeros(0u) == 32);
    CHECK(stdc_leading_ones((unsigned short)0xf000) == 4);
    CHECK(stdc_trailing_zeros(8u) == 3 && stdc_trailing_zeros(0ull) == 64);
    CHECK(stdc_trailing_ones(7u) == 3);
    CHECK(stdc_first_leading_zero(0xffffffffu) == 0 && stdc_first_leading_zero(0u) == 1);
    CHECK(stdc_first_leading_one(1u) == 32 && stdc_first_leading_one(0u) == 0);
    CHECK(stdc_first_trailing_zero(1u) == 2 && stdc_first_trailing_one(8u) == 4);
    CHECK(stdc_count_ones(0xf0f0u) == 8 && stdc_count_zeros((unsigned char)1) == 7);
    CHECK(stdc_has_single_bit(64u) && !stdc_has_single_bit(65u) && !stdc_has_single_bit(0u));
    CHECK(stdc_bit_width(255u) == 8 && stdc_bit_width(0u) == 0);
    CHECK(stdc_bit_floor(100u) == 64 && stdc_bit_ceil(100u) == 128 && stdc_bit_ceil(0u) == 1);
    CHECK(stdc_bit_ceil((unsigned char)200) == 0);
    CHECK(stdc_count_ones_ull(~0ull) == 64 && stdc_bit_ceil_us(3) == 4);
    CHECK(sizeof(stdc_bit_floor((unsigned char)3)) == 1);
    CHECK(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__);

    /* <stdckdint.h> */
    int ri; unsigned ru; long long rl; unsigned long long rul; signed char rc;
    CHECK(!ckd_add(&ri, 1, 2) && ri == 3);
    CHECK(ckd_add(&ri, INT_MAX, 1) && ri == INT_MIN);
    CHECK(ckd_sub(&ru, 1u, 2u) && ru == UINT_MAX);
    CHECK(!ckd_sub(&ri, 1u, 2u) && ri == -1);
    CHECK(!ckd_mul(&rl, 3000000000u, 3) && rl == 9000000000LL);
    CHECK(ckd_mul(&ri, 65536, 65536) && ri == 0);
    CHECK(ckd_mul(&rl, LLONG_MAX, 2) && rl == -2);
    CHECK(!ckd_mul(&rl, LLONG_MIN, 1) && rl == LLONG_MIN);
    CHECK(ckd_mul(&rl, LLONG_MIN, -1));
    CHECK(!ckd_mul(&rul, 0xffffffffull, 0x100000001ull) && rul == ~0ull);
    CHECK(ckd_add(&rul, ~0ull, 1) && rul == 0);
    CHECK(ckd_add(&rul, 5, -6) && !ckd_add(&rl, 5, -6) && rl == -1);
    CHECK(ckd_add(&rc, 100, 100) && rc == -56 && !ckd_sub(&rc, -100, 28) && rc == -128);

    puts(err ? "c23_more: FAILED" : "c23_more: ok");
    return err;
}
