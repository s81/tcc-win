/* C23 constexpr objects of structure, union and array type: their members
   and elements are constant expressions */
#include <stdio.h>
#include <stdbool.h>

struct P { int x, y; };
struct N { signed char sc; unsigned char uc; short s; bool b; float f; double d;
           int arr[3]; struct P p; };
union U { int i; unsigned u; };
struct B { int *ptr; unsigned bits : 4; };

constexpr struct P gp = { 3, -4 };
constexpr struct N gn = { -1, 255, -300, true, 1.5f, -2.25, { 10, 20, 30 }, { 7, 8 } };
constexpr int ga[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
constexpr struct P gps[2] = { { 1, 2 }, { 3, 4 } };
constexpr union U gu = { .i = -1 };
constexpr struct P gz = { 0 };

/* used in constant expressions */
static_assert(gp.x == 3 && gp.y == -4);
static_assert(gn.sc == -1 && gn.uc == 255 && gn.s == -300 && gn.b);
static_assert(gn.f == 1.5f && gn.d == -2.25);
static_assert(gn.arr[2] == 30 && gn.p.y == 8);
static_assert(ga[1][2] == 6 && gps[1].x == 3);
static_assert(gu.i == -1);
static_assert(gz.x == 0 && gz.y == 0);
static int sized[gn.arr[0]];
static_assert(sizeof sized == 10 * sizeof(int));
enum { E = gp.x * 100 + ga[0][1] };
static_assert(E == 302);
constexpr int k = gps[0].y + gn.p.x;          /* constexpr from constexpr */
static_assert(k == 9);

static int sw(int v)
{
    switch (v) {
    case gp.x: return 1;                    /* case label */
    case ga[1][0]: return 2;
    default: return 0;
    }
}

static int other(void) { return 42; }
constexpr struct B gb = { nullptr, 5 };

int main(void)
{
    constexpr struct P lp = { 5, 6 };       /* block scope */
    static_assert(lp.x + lp.y == 11);
    int i = 1;
    int fail = 0;

    /* addresses still refer to the object */
    const int *px = &gp.x, *py = &(gp.y), *pa = &ga[1][gp.x - 3];
    fail |= *px != 3 || *py != -4 || *pa != 4;
    fail |= px + 1 != py;
    /* run-time index: an ordinary load */
    fail |= ga[i][i] != 5 || gps[i].y != 4;
    /* members that are not constants are still read */
    fail |= gb.ptr != nullptr || gb.bits != 5;
    fail |= sw(3) != 1 || sw(4) != 2 || sw(9) != 0;
    fail |= other() != 42 || gu.u != 0xffffffffu;
    printf("%s\n", fail ? "FAIL" : "constexpr aggregates ok");
    return fail;
}
