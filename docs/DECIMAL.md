# Decimal floating point (`<tcc_decimal.h>`)

tcc-win ships an IEEE 754-2019 decimal floating-point library in its runtime
library, `libtcc1.a`. It provides decimal32, decimal64 and decimal128 numbers
in the BID (binary integer decimal) encoding, with correctly rounded
arithmetic, all five IEEE rounding modes and the five IEEE exception flags.

Use it when values must behave like the decimal numbers people write: money,
prices, tax rates, measurements that are recorded in decimal. In decimal,
`0.1 + 0.2` is exactly `0.3`, and `19.99 * 3` is exactly `59.97`.

The language types `_Decimal32`, `_Decimal64` and `_Decimal128` are **not**
supported: there are no decimal literals (`1.5DD`) and no `+ - * /`
operators on decimal values. You call functions instead. The library is
Windows-only, like the rest of tcc-win.

## Contents

- [Getting started](#getting-started)
- [Formats](#formats)
- [Contexts: rounding and flags](#contexts-rounding-and-flags)
- [Function reference](#function-reference)
- [Strings](#strings)
- [Special values](#special-values)
- [Behaviour IEEE 754 leaves open](#behaviour-ieee-754-leaves-open)
- [Limits](#limits)
- [Tests](#tests)

## Getting started

Include the header. No link option is needed: the functions are in
`libtcc1.a`, which every program built by tcc-win links, so they work both
for executables and under `-run`.

```c
#include <stdio.h>
#include <tcc_decimal.h>

int main(void)
{
    tcc_dec_ctx ctx = { TCC_DEC_NEAREST_EVEN, 0 };
    tcc_dec64 price = tcc_strtod64("19.99", NULL);
    tcc_dec64 qty   = tcc_d64_from_i64(3, &ctx);
    tcc_dec64 total = tcc_d64_mul(price, qty, &ctx);
    tcc_dec64 cents = tcc_strtod64("0.01", NULL);
    tcc_dec64 tax   = tcc_d64_mul(total, tcc_strtod64("0.0825", NULL), &ctx);
    char buf[64];

    tcc_d64_to_string(buf, sizeof buf, total, NULL);
    printf("total  %s\n", buf);                       /* 59.97 */
    tax = tcc_d64_quantize(tax, cents, &ctx);         /* round to cents */
    tcc_d64_to_string(buf, sizeof buf, tax, NULL);
    printf("tax    %s\n", buf);                       /* 4.95 */
    printf("flags  %s\n", ctx.flags & TCC_DEC_INEXACT ? "inexact" : "exact");

    /* 0.1 + 0.2 is exactly 0.3 in decimal */
    ctx.flags = 0;
    tcc_dec64 s = tcc_d64_add(tcc_strtod64("0.1", NULL), tcc_strtod64("0.2", NULL), &ctx);
    printf("equal  %d\n", tcc_d64_compare(s, tcc_strtod64("0.3", NULL), &ctx) == 0);
    return 0;
}
```

```
tcc money.c -o money.exe      or      tcc -run money.c
```

Output:

```
total  59.97
tax    4.95
flags  inexact
equal  1
```

`quantize` rounded `4.947525` to two decimal places and raised Inexact
because digits were dropped.

## Formats

| Type | Format | Significant digits | Exponent range (of the last digit) | Size |
|---|---|---|---|---|
| `tcc_dec32` | decimal32 | 7 | -101 to 90 | 4 bytes |
| `tcc_dec64` | decimal64 | 16 | -398 to 369 | 8 bytes |
| `tcc_dec128` | decimal128 | 34 | -6176 to 6111 | 16 bytes |

Each type is a small struct that holds the IEEE bit pattern (`w` field;
`tcc_dec128` keeps its low half in `w[0]`). Pass and return them by value.
Do not do arithmetic on the bits; compare with `tcc_d*_compare`, not with
`==` on `w`, because one value can have several encodings (see
[cohorts](#strings)).

Function names start with the format: `tcc_d32_`, `tcc_d64_` or
`tcc_d128_`. The three sets are identical apart from the type.

## Contexts: rounding and flags

Every operation takes a pointer to a context as its last argument:

```c
typedef struct { int rounding; unsigned flags; } tcc_dec_ctx;
```

`rounding` is one of:

| Constant | Rounds to |
|---|---|
| `TCC_DEC_NEAREST_EVEN` | nearest, ties to even (the default) |
| `TCC_DEC_NEAREST_AWAY` | nearest, ties away from zero |
| `TCC_DEC_TOWARD_ZERO` | toward zero (truncate) |
| `TCC_DEC_UPWARD` | toward +infinity |
| `TCC_DEC_DOWNWARD` | toward -infinity |

`flags` collects exceptions. Operations only ever **set** bits; nothing
clears them, so clear them yourself (`ctx.flags = 0`) before the work you
want to check.

| Flag | Raised when |
|---|---|
| `TCC_DEC_INVALID` | the operation has no meaningful result (`0/0`, `sqrt(-1)`, a signaling NaN operand, a conversion out of range) |
| `TCC_DEC_DIVBYZERO` | a finite non-zero number is divided by zero |
| `TCC_DEC_OVERFLOW` | the rounded result is too large for the format |
| `TCC_DEC_UNDERFLOW` | the result is tiny **and** inexact |
| `TCC_DEC_INEXACT` | the result had to be rounded |

### The per-thread default

Pass `NULL` instead of a context to use the calling thread's default
context, returned by `tcc_dec_thread_ctx()`. Each thread gets its own,
starting with `TCC_DEC_NEAREST_EVEN` and no flags, so threads never see
each other's rounding mode or flags.

```c
tcc_dec_thread_ctx()->rounding = TCC_DEC_UPWARD;
q = tcc_d64_div(one, three, NULL);           /* 0.3333333333333334 */
```

The default context costs 8 bytes of Win32 thread-local storage per thread
that uses it and is never freed.

## Function reference

The list shows decimal64; replace `d64` and `tcc_dec64` for the other
formats. `T` is the format's type and every function takes a
`tcc_dec_ctx *ctx` last (may be `NULL`, see above) unless noted.

### Arithmetic

All are correctly rounded: the result is the exact mathematical result,
rounded once by the context's mode.

| Function | Result |
|---|---|
| `T tcc_d64_add(T a, T b, ctx)` | `a + b` |
| `T tcc_d64_sub(T a, T b, ctx)` | `a - b` |
| `T tcc_d64_mul(T a, T b, ctx)` | `a * b` |
| `T tcc_d64_div(T a, T b, ctx)` | `a / b` |
| `T tcc_d64_sqrt(T a, ctx)` | square root |
| `T tcc_d64_fma(T a, T b, T c, ctx)` | `a * b + c`, rounded once |
| `T tcc_d64_quantize(T a, T q, ctx)` | `a` rounded to the exponent of `q` (for example `q = 0.01` rounds to cents); Invalid if the result needs more digits than the format has |
| `int tcc_d64_compare(T a, T b, ctx)` | `-1`, `0` or `1`; `2` if either is a NaN (unordered). Only a signaling NaN raises Invalid |

### Conversions

| Function | Notes |
|---|---|
| `T tcc_d64_from_i64(int64_t, ctx)`, `from_u64(uint64_t, ctx)` | exact when the integer fits in the format's digits, otherwise rounded |
| `T tcc_d64_from_double(double, ctx)`, `from_float(float, ctx)` | the exact binary value, rounded once. The double `0.1` is slightly above 0.1, so `tcc_d64_from_double(0.1)` gives `0.1000000000000000` with Inexact, and `tcc_d128_from_double(0.1)` gives `0.1000000000000000055511151231257827`. Parse a string to get exactly 0.1 |
| `int64_t tcc_d64_to_i64(T, ctx)`, `to_u64`, `to_i32`, `to_u32` | rounded by the context's mode (not truncated, unlike a C cast); Inexact if a fraction was lost; NaN gives 0, out-of-range values and infinities saturate, and both raise Invalid |
| `double tcc_d64_to_double(T, ctx)`, `float tcc_d64_to_float(T, ctx)` | correctly rounded to binary |
| `tcc_dec32 tcc_d64_to_d32(T, ctx)`, `to_d64`, `to_d128` | between the decimal formats; widening is exact |

To truncate like a C cast, use a context whose rounding is
`TCC_DEC_TOWARD_ZERO`:

```c
tcc_dec_ctx trunc = { TCC_DEC_TOWARD_ZERO, 0 };
int64_t whole = tcc_d64_to_i64(x, &trunc);
```

## Strings

```c
T   tcc_d64_from_string(const char *s, char **end, ctx);
int tcc_d64_to_string(char *buf, size_t n, T x, const char *fmt);   /* no context */

tcc_dec32  tcc_strtod32 (const char *s, char **end);    /* C23 names, */
tcc_dec64  tcc_strtod64 (const char *s, char **end);    /* default context */
tcc_dec128 tcc_strtod128(const char *s, char **end);
```

**Parsing** works like `strtod`: leading white space, an optional sign,
`digits[.digits][e[sign]digits]`, or `inf`, `infinity`, `nan`, `nan(digits)`,
`nandigits`, `snan` (case does not matter). `*end` is set to the first
character not used, or to `s` if nothing was parsed. Numbers with more
digits than the format holds are correctly rounded.

**Formatting**: `fmt` `NULL`, `""` or `"s"` gives the scientific form of
IEEE 754's to-scientific-string, which keeps the exponent (the *cohort*), so
`1.20E+3` and `1.2E+3` print differently although they compare equal.
`"f"` gives plain digits with no exponent:

| Value parsed from | `NULL` | `"f"` |
|---|---|---|
| `"1.20E+3"` | `1.20E+3` | `1200` |
| `"0.00123"` | `0.00123` | `0.00123` |
| `"-inf"` | `-Infinity` | `-Infinity` |
| `"nan12"` | `NaN12` | `NaN12` |

`to_string` returns the length the whole text needs, like `snprintf`, and
never writes more than `n` bytes including the terminating zero. 64 bytes
are enough for any decimal64 in the scientific form. In the `"f"` form a
large exponent produces a long string: call with `n = 0` first to get the
size.

## Special values

Infinities and NaNs follow IEEE 754:

- `1/0` is infinity with `TCC_DEC_DIVBYZERO`; `0/0`, `inf - inf` and
  `sqrt(-1)` are a quiet NaN with `TCC_DEC_INVALID`.
- A NaN carries a sign and a payload (`NaN12`). Results propagate the first
  signaling NaN operand (made quiet), otherwise the first quiet NaN.
- Signed zeros are kept (`-0`).

## Behaviour IEEE 754 leaves open

Where the standard allows a choice, the library does this:

- Tininess is detected **before** rounding; Underflow is raised only when
  the result is both tiny and inexact.
- A non-canonical encoding (a coefficient too large for the format) reads as
  zero.
- NaN payloads that do not fit the destination format keep their low digits.
  Invalid operations return the positive quiet NaN with payload 0.
- `quantize` raises Inexact when it drops non-zero digits.
- `x / infinity` is zero with the smallest exponent.

## Limits

- No language support: no `_Decimal32/64/128` types, decimal literals,
  operators, `printf` conversions (`%Da`, `%Df`, `%DDf`) or `<math.h>`
  decimal functions. `__STDC_IEC_60559_DFP__` is not defined.
- Only the BID encoding; DPD (densely packed decimal) is not provided.
- The library is pure C with no assembly. decimal32 and decimal64 `+ - * /`
  and `sqrt` have 64-bit fast paths; decimal128 always uses the general
  multi-word path and is slower.
- The `nan(...)` and `snan` spellings, and the `"f"` format, are this
  library's choices: other decimal libraries may print or accept different
  text.

## Tests

```bash
sh tests/decimal/run.sh          # 2307 test vectors, all three formats and all rounding modes
```

`tests/jitbench/suites.sh` also runs the unit tests
(`tests/decimal/decunit.c`: per-thread contexts, `strtod` names, hand-checked
values) and compiles the library for i386 and arm64. More vectors can be
generated with `tests/decimal/gen_vectors.py` (needs Python).

The implementation is `lib/decbid.c`; its header comment explains the
design (exact integer arithmetic, one rounding step for every operation).
