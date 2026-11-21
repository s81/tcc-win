/* Gaps found by the final C23 check:
   - 'auto' with a struct, union or enum without a tag in its initializer
   - iswblank declared by <wctype.h> alone (also after <wchar.h>) */
#include <wchar.h>
#include <wctype.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

auto gq = (struct { int a; struct { int b; } in; }){ 1, { 2 } };
auto ge = (enum { Z1, Z2 }) 1;
static_assert(Z2 == 1);

static int f(void)
{
    auto q = (struct { int a; }){ 1 };
    auto u = (union { int a; char c; }){ .a = 7 };
    auto e = (enum { Y1 = 3 }) Y1;
    auto p = (struct { int a; } *)0;
    auto s = ({ auto r = (struct { int k; }){ 40 }; r.k; });
    (void)p;
    return q.a + u.a + e + gq.a + gq.in.b + ge + s;
}

int main(void)
{
    CHECK(f() == 1 + 7 + 3 + 1 + 2 + 1 + 40);
    CHECK(gq.in.b == 2);
    CHECK(iswblank(L' ') && iswblank(L'\t') && !iswblank(L'a'));
    return 0;
}
