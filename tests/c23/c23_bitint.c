/* C23 _BitInt(N) with N <= 64 and wb/uwb constants. Exits with 0 on success. */
#include <stdio.h>
#include <limits.h>

static int err;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); err = 1; } } while (0)

static_assert(sizeof(_BitInt(7)) == 1 && sizeof(_BitInt(9)) == 2);
static_assert(sizeof(_BitInt(17)) == 4 && sizeof(unsigned _BitInt(33)) == 8);
static_assert(sizeof(_BitInt(64)) == 8);
static_assert((_BitInt(5))15wb + 1wb == -16);      /* constant expressions wrap */
static_assert((unsigned _BitInt(3))9 == 1);
static_assert(__BITINT_MAXWIDTH__ >= 64);

struct S { _BitInt(12) a; unsigned _BitInt(40) b; };
static unsigned _BitInt(24) g24 = 0xffffffuwb;

static _BitInt(24) mul24(_BitInt(24) a, _BitInt(24) b) { return a * b; }
static unsigned _BitInt(7) inc7(unsigned _BitInt(7) x) { return x + 1; }

int main(void)
{
    volatile int one = 1;                    /* defeat constant folding */
    unsigned _BitInt(7) u7 = 127;
    _BitInt(7) s7 = 63;
    _BitInt(24) s24 = 0x7fffff;
    unsigned _BitInt(40) u40 = 0xffffffffffuwb;
    _BitInt(64) s64 = LLONG_MAX;
    unsigned _BitInt(1) u1 = 1;
    struct S s = { 2047, 1 };

    /* no integer promotion: arithmetic wraps at N bits */
    CHECK(u7 + (unsigned _BitInt(7))one == 0);
    CHECK(sizeof(u7 + u7) == 1);
    CHECK(inc7(u7) == 0 && inc7(5) == 6);
    u7 += one;
    CHECK(u7 == 0);
    u7--;
    CHECK(u7 == 127);
    CHECK((unsigned _BitInt(7))~u7 == 0 && -(unsigned _BitInt(7))one == 127);

    /* signed: conversion keeps the low N bits (two's complement) */
    s7 = (_BitInt(7))(64 * one);
    CHECK(s7 == -64);
    s7 = 63;
    s7 = s7 + (_BitInt(7))one;
    CHECK(s7 == -64);
    s24++;
    CHECK(s24 == -0x800000);
    CHECK(mul24(0x1000, 0x1000) == 0 && mul24(-3, 5) == -15);
    CHECK(s24 >> 4 == -0x80000 && (s24 >> 23) == -1);

    /* wide types */
    u40 += one;
    CHECK(u40 == 0);
    u40 = (unsigned _BitInt(40))1 << 39;
    CHECK(u40 == 0x8000000000ull && u40 << 1 == 0);
    s64 += one;
    CHECK(s64 == LLONG_MIN);
    u1 += one;
    CHECK(u1 == 0);

    /* mixing with standard types: the wider type wins */
    CHECK(sizeof(s7 + 1) == sizeof(int) && sizeof(s7 + 1LL) == 8);
    CHECK(sizeof(u40 + 1) == 8 && u7 + 200 == 327);
    CHECK(_Generic(s7 + s7, _BitInt(7): 1, int: 2, default: 3) == 1);
    CHECK(_Generic(s24 + s7, _BitInt(24): 1, default: 3) == 1);
    CHECK(_Generic(s7, _BitInt(8): 1, _BitInt(7): 2, signed char: 3, default: 4) == 2);
    CHECK(_Generic(u7, unsigned _BitInt(7): 1, default: 2) == 1);
    CHECK((double)s24 == -8388608.0 && (_BitInt(7))65.0 == -63);

    /* constants: the smallest type that holds the value */
    CHECK(_Generic(3wb, _BitInt(3): 1, default: 0));
    CHECK(_Generic(3uwb, unsigned _BitInt(2): 1, default: 0));
    CHECK(_Generic(0wb, _BitInt(2): 1, default: 0) && _Generic(0WBU, unsigned _BitInt(1): 1, default: 0));
    CHECK(_Generic(-3wb, _BitInt(3): 1, default: 0) && -3wb == -3);
    CHECK(0xffwb == 255 && sizeof(0x1ffffffffwb) == 8);

    /* members, globals, comparisons, division */
    s.a++;
    s.b = s.b - 2;
    CHECK(s.a == -2048 && s.b == 0xffffffffffull);
    CHECK(g24 + (unsigned _BitInt(24))one == 0 && g24 > 5 && s24 < 0);
    CHECK(s24 / 2 == -0x400000 && s24 % 3 == -2 && g24 / 0xffff == 256);

    puts(err ? "c23_bitint: FAILED" : "c23_bitint: ok");
    return err;
}
