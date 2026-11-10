/* C23 gaps found by the conformance audit (docs/C23-STATUS.md), fixed.
   Compiled with -Wall: it must not produce any diagnostic. */
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* tag compatibility: an identical redefinition in the same scope */
struct T { int a; char b[3]; unsigned bits : 5; };
struct T { int a; char b[3]; unsigned bits : 5; };
union U { int i; float f; };
union U { int i; float f; };
enum E { E0, E1 = 5, E2 };
enum E { E0, E1 = 5, E2 };
enum F : short { F0 = -1 };
enum F : short { F0 = -1 };
static_assert(sizeof(struct T) == 8 && E2 == 6 && sizeof(enum F) == 2);

/* u8 character constants have type unsigned char */
static_assert(_Generic(u8'a', unsigned char: 1, default: 0));
static_assert(u8'\xff' == 255 && sizeof u8'z' == 1);
#define STR(x) #x
static_assert(sizeof STR(u8'a') == sizeof "u8'a'");

/* #embed with the resource name from a macro */
#define RESOURCE "embed.txt"
static const unsigned char data[] = {
#embed RESOURCE
};
#if !__has_embed(RESOURCE)
#error __has_embed with a macro
#endif

/* header additions */
static_assert(__STDC_VERSION_LIMITS_H__ == 202311L);
static_assert(__STDC_VERSION_STDINT_H__ == 202311L && __STDC_VERSION_ASSERT_H__ == 202311L);
static_assert(INT32_WIDTH == 32 && UINT64_WIDTH == 64 && INT_FAST8_WIDTH == 8);
static_assert(SIZE_WIDTH == sizeof(size_t) * CHAR_BIT && INTPTR_WIDTH == sizeof(void *) * CHAR_BIT);
static_assert(FLT_IS_IEC_60559 == 1 && DBL_NORM_MAX == DBL_MAX);

struct P { int x, y; };

int main(void)
{
    struct T t = { 1, "ab", 3 };
    union U u = { .i = 0 };
    enum E e = E2;
    float inf = INFINITY, nan = NAN;
    int fail = 0;

    fail |= t.a != 1 || t.bits != 3 || u.i != 0 || e != 6;
    fail |= sizeof data == 0;
    fail |= !(inf > FLT_MAX) || nan == nan;
    assert((struct P){ 1, 2 }.y == 2);  /* comma in the argument */
    {
        fail |= 0;
    end:                                /* label at the end of a block */
    }
    printf("%s\n", fail ? "FAIL" : "audit fixes ok");
    return fail;
}
