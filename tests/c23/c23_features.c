/* C23 features supported by tcc-win. Exits with 0 on success. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if __STDC_VERSION__ != 202311L
# error "C23 must be the default"
#endif
#if !__has_include(<stddef.h>) || !defined(__has_c_attribute) \
    || !__has_c_attribute(nodiscard) || !__has_embed("embed.txt")
# error "__has_include/__has_c_attribute/__has_embed"
#endif
#if true && !false
#else
# error "true/false in #if"
#endif
#ifdef NOPE
#elifndef ALSO_NOPE
# define ELIFNDEF_OK 1
#endif

#ifndef __TCC_NO_GOTO__
# error "__TCC_NO_GOTO__ must be defined without -fgoto"
#endif
static_assert(ELIFNDEF_OK);                 /* no message */
static_assert(sizeof(bool) == 1, "bool");
static_assert(alignof(int) == 4);
enum E : unsigned char { EA, EB };         /* fixed underlying type */
static_assert(sizeof(enum E) == 1);
constexpr int N = 0b1'000;                 /* constexpr, binary literal, digit separator */
static_assert(N == 8);
static int arr[N];
alignas(16) static int aligned_var;
thread_local static int tls_var = 1;
static const unsigned char embedded[] = {
#embed "embed.txt"
};

[[nodiscard]] static int twice(int x) { return 2 * x; }
[[maybe_unused]] static void unused(void) {}
[[deprecated("old")]] static int old(void) { return 1; }
static int unnamed(int, char *) { return 7; }   /* unnamed parameters */

int main(void)
{
    int err = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); err = 1; } } while (0)
    int *np = nullptr;
    nullptr_t nt = nullptr;
    auto d = 1.5;                             /* type inference */
    auto p = &d;
    constexpr unsigned char small = 300 - 50;
    const int ci = 1;
    typeof_unqual(ci) mut = 2;
    typeof(ci) *pc = &ci;
    struct S { int a, b; } s = {};            /* empty initializer */
    int z = {};
    int a3[3] = {};
    bool b = true;

    mut = 3;
    CHECK(np == nullptr && nt == nullptr);
    CHECK(sizeof d == sizeof(double) && *p > 1.4);
    CHECK(small == 250 && sizeof small == 1);
    CHECK(mut == 3 && *pc == 1);
    CHECK(s.a == 0 && s.b == 0 && z == 0 && a3[2] == 0);
    CHECK(b && !false && sizeof(true) == 1);
    CHECK(1'000'000 == 1000000 && 0x1'ff == 511);
    CHECK(u8'a' == 97);
    CHECK(sizeof arr == 8 * sizeof(int));
    CHECK(((size_t)&aligned_var & 15) == 0 && tls_var == 1);
    CHECK(sizeof embedded == 6 && !memcmp(embedded, "embed\n", 6));
    CHECK(twice(4) == 8 && old() == 1 && unnamed(0, nullptr) == 7);
    switch (z) {
    case 0:
        z++;
        [[fallthrough]];
    default:
        z++;
    }
    CHECK(z == 2);
end:                                          /* a label needs no goto */
    int after_label = 1;                      /* declaration after label */
    CHECK(after_label);
    if (0)
        unreachable();
    puts(err ? "c23: FAILED" : "c23: ok");
    return err;
}
