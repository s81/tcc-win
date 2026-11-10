# C23 conformance status

The results of an audit of tcc-win against the C23 standard (ISO/IEC 9899:2024).
Each feature was checked with a small test program compiled (and, for library
functions, linked) by the x86_64-win32 compiler against the headers and
libraries shipped with tcc-win. A test passes when it compiles and links
without diagnostics and its `static_assert`s hold.

The audit covers the language and library changes that C23 made relative to
C17. It is not exhaustive: optional features (decimal floating point
`_Decimal32/64/128`, Annex F/G/H details) were not tested.

## Language: supported

| Feature | Notes |
|---|---|
| `nullptr`, `nullptr_t` | `nullptr_t` in `<stddef.h>` |
| `constexpr` | scalars; members and elements of structs, unions and arrays |
| `auto` type inference, `typeof`, `typeof_unqual` | |
| `bool`, `true`, `false`, `alignas`, `alignof`, `static_assert`, `thread_local` as keywords | `true`/`false` have type `bool` |
| `static_assert` without a message | |
| `[[deprecated]]`, `[[fallthrough]]`, `[[maybe_unused]]`, `[[nodiscard]]`, `[[noreturn]]`, `[[unsequenced]]`, `[[reproducible]]` | also `[[__name__]]` spellings, `__has_c_attribute` |
| empty initializer `= {}` | |
| enums with a fixed underlying type, enumerators wider than `int` | |
| unnamed parameters in function definitions | |
| labels before declarations | |
| `static` / `constexpr` compound literals | |
| `f()` means `f(void)`; `f(...)`; `va_start(ap)` | old-style definitions removed |
| binary literals, digit separators `'` | |
| `u8` character constants | type `unsigned char`; one UTF-8 code unit |
| `_BitInt(N)`, `wb`/`uwb` suffixes | `BITINT_MAXWIDTH` is 64, the minimum C23 requires |
| `#elifdef`, `#elifndef`, `#warning`, `__VA_OPT__`, `__has_include` | |
| `#embed` with `limit`, `prefix`, `suffix`, `if_empty`; `__has_embed` | the resource name may come from a macro |
| tag compatibility: an identical redefinition of a struct, union or enum in the same scope (N3037) | a different one is an error |
| labels at the end of a compound statement | `-Wall` warns only before C23 |
| `__STDC_VERSION__` = `202311L` | |
| `<stdbit.h>`, `<stdckdint.h>`, `unreachable()` | |
| `<limits.h>` `*_WIDTH` macros, `BITINT_MAXWIDTH` | |
| `<stdint.h>` `*_WIDTH` macros | |
| `assert(...)` is variadic | commas, e.g. in compound literals, are allowed |
| `INFINITY`, `NAN`, `*_IS_IEC_60559`, `*_NORM_MAX` in `<float.h>` | |
| `__STDC_VERSION_*_H__` = `202311L` | defined only in the headers that are complete: `<assert.h>`, `<limits.h>`, `<stdarg.h>`, `<stdbit.h>`, `<stdckdint.h>`, `<stddef.h>`, `<stdint.h>` |

## Language: gaps

None found by this audit.

## Headers: gaps

| Gap | Observed |
|---|---|
| `FLT_SNAN`, `DBL_SNAN`, `LDBL_SNAN` in `<float.h>` | not defined: tcc has no constant for a signaling NaN. For this reason `__STDC_VERSION_FLOAT_H__` is not defined |
| `__STDC_VERSION_*_H__` of `<stdlib.h>`, `<string.h>`, `<stdio.h>`, `<time.h>`, `<math.h>`, `<uchar.h>`, `<wchar.h>`, `<inttypes.h>`, `<setjmp.h>`, `<fenv.h>`, `<complex.h>`, `<tgmath.h>`, `<stdatomic.h>` | deliberately not defined: the macro states that the header is C23-complete, and these lack the library functions below (or were not audited) |

## Library: supported

tcc-win links against `msvcrt.dll`, the Windows system C runtime, which
predates C11/C23. The following functions are provided by tcc-win's runtime
library (`libtcc1.a`, sources `lib/c23rt.c` and `lib/c23math.c`) and declared in the shipped
headers when `__STDC_VERSION__` is at least `202311L` (`201112L` for
`timespec_get` and `TIME_UTC`). Tested by `tests/c23/c23_runtime.c` and
`tests/c23/c23_math2.c`.

| Function(s) | Header | Notes |
|---|---|---|
| `memset_explicit`, `strndup` | `<string.h>` | `strdup` comes from msvcrt |
| `memalignment` | `<stdlib.h>` | |
| `strfromd`, `strfromf`, `strfroml` | `<stdlib.h>` | formatted by msvcrt; infinities and NaNs are written as `inf`/`nan` (`INF`/`NAN`); `%a`/`%A` output is msvcrt's |
| `gmtime_r`, `localtime_r`, `timegm` | `<time.h>` | via `_gmtime64_s`, `_localtime64_s`, `_mkgmtime64`; `time_t` is 32 bits on i386 |
| `timespec_get` with `TIME_UTC` (C11), `timespec_getres` (C23) | `<time.h>` | `GetSystemTimePreciseAsFileTime` (Windows 8+), else `GetSystemTimeAsFileTime`; resolution reported as 100 ns |
| `roundeven`, `nextup`, `nextdown` | `<math.h>` | with `f` and `l` variants |
| `fmaximum`, `fminimum`, `fmaximum_num`, `fminimum_num`, `fmaximum_mag`, `fminimum_mag`, `fmaximum_mag_num`, `fminimum_mag_num` | `<math.h>` | with `f` and `l` variants; `long double` is `double` on these targets |
| `sinpi`, `cospi`, `tanpi`, `asinpi`, `acospi`, `atanpi`, `atan2pi` | `<math.h>` | with `f` and `l` variants (as all below); exact at integers and half-integers, signed zeros and poles as C23 Annex F specifies; built on msvcrt's `sin`, `cos`, ... (within about 2 ulps) |
| `exp10`, `exp10m1`, `exp2m1`, `logp1`, `log2p1`, `log10p1` | `<math.h>` | exact for integral `exp10`/`exp10m1`/`exp2m1` arguments and exact powers in `log2p1`/`log10p1`; otherwise within a few ulps |
| `pown`, `compoundn`, `rootn`, `powr`, `rsqrt` | `<math.h>` | `pown`, `compoundn` and `rootn` use double-double arithmetic: exact results are exact (`rootn(-8, 3) == -2`), others within about 0.5 ulp |
| `fromfp`, `ufromfp`, `fromfpx`, `ufromfpx`, `FP_INT_UPWARD`, `FP_INT_DOWNWARD`, `FP_INT_TOWARDZERO`, `FP_INT_TONEARESTFROMZERO`, `FP_INT_TONEAREST` | `<math.h>` | C23 form (returns the floating type; NaN when out of range) |
| `canonicalize`, `iscanonical`, `issignaling`, `issubnormal`, `iszero` | `<math.h>` | every encoding is canonical; `canonicalize` quiets a signaling NaN; on i386 x87 loads quiet a signaling NaN before `issignaling` sees it |

The `<math.h>` functions set neither `errno` nor the floating-point
exception flags (`fromfpx` behaves like `fromfp`).

The `__STDC_VERSION_*_H__` macros are still not defined, since the headers
remain incomplete.

## Library: gaps

These are not declared and not available:

| Function(s) | Standard |
|---|---|
| `free_sized`, `free_aligned_sized` | C23; not provided together with `aligned_alloc` (below) |
| `aligned_alloc` | C11; memory from msvcrt's `_aligned_malloc` cannot be released with `free`, so a compatible version needs its own allocator |
| `llogb`, `FP_LLOGB0`, `FP_LLOGBNAN`, `totalorder`, `totalordermag`, `getpayload`, `setpayload`, `setpayloadsig`, narrowing `fadd`/`dadd`/`fsub`/`fmul`/`fdiv`/`ffma`/`fsqrt` (and variants) | C23 `<math.h>` |
| `%b` in the `printf` family | C23; compiles, but msvcrt's `printf` does not implement it (not verified at run time) |
