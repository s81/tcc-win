/* gaps found by the final audit: iswblank from <wctype.h> alone (also after
   <ctype.h>), auto with an unnamed struct or union type */
#include <wctype.h>
#include <wchar.h>
#include <ctype.h>

int main(void)
{
    auto q = (struct { int a; long b; }){ 1, 2 };
    auto r = q;
    auto u = (union { int a; char c; }){ 7 };
    auto n = (struct { int a; struct { int b; } n; }){ 1, { 2 } };

    r.a += 10;
    if (!iswblank(L' ') || iswblank(L'x'))
        return 1;
    return !(q.a == 1 && q.b == 2 && r.a == 11 && u.a == 7 && n.n.b == 2);
}
