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
| `u8` character constants | but see the type below |
| `_BitInt(N)`, `wb`/`uwb` suffixes | `BITINT_MAXWIDTH` is 64, the minimum C23 requires |
| `#elifdef`, `#elifndef`, `#warning`, `__VA_OPT__`, `__has_include` | |
| `#embed` with `limit`, `prefix`, `suffix`, `if_empty`; `__has_embed` | but see macro names below |
| `__STDC_VERSION__` = `202311L` | |
| `<stdbit.h>`, `<stdckdint.h>`, `unreachable()` | |
| `<limits.h>` `*_WIDTH` macros, `BITINT_MAXWIDTH` | |

## Language: gaps

| Gap | Observed |
|---|---|
| Tag compatibility: a second, identical definition of a struct, union or enum in the same scope (N3037) | `error: struct/union/enum already defined` |
| Type of `u8'a'` should be `unsigned char` | it is `int` (`_Generic(u8'a', int: ...)` matches) |
| `#embed` with a macro that expands to the resource name | `error: '#embed' expects "FILENAME" or <FILENAME>` (a literal name works) |
| Label at the end of a compound statement is valid C23 | `-Wall` warns "deprecated use of label at end of compound statement" |

## Headers: gaps

| Gap | Observed |
|---|---|
| `__STDC_VERSION_STDLIB_H__` and the other `__STDC_VERSION_*_H__` macros | not defined |
| `INFINITY`, `NAN` (and the other new macros) in `<float.h>` | not defined there (they are in `<math.h>`) |
| `*_WIDTH` macros in `<stdint.h>` (`INT32_WIDTH`, `INTPTR_WIDTH`, ...) | not defined |

## Library: supported

tcc-win links against `msvcrt.dll`, the Windows system C runtime, which
predates C11/C23. The following functions are provided by tcc-win's runtime
library (`libtcc1.a`, source `lib/c23rt.c`) and declared in the shipped
headers when `__STDC_VERSION__` is at least `202311L` (`201112L` for
`timespec_get` and `TIME_UTC`). Tested by `tests/c23/c23_runtime.c`.

| Function(s) | Header | Notes |
|---|---|---|
| `memset_explicit`, `strndup` | `<string.h>` | `strdup` comes from msvcrt |
| `memalignment` | `<stdlib.h>` | |
| `strfromd`, `strfromf`, `strfroml` | `<stdlib.h>` | formatted by msvcrt; infinities and NaNs are written as `inf`/`nan` (`INF`/`NAN`); `%a`/`%A` output is msvcrt's |
| `gmtime_r`, `localtime_r`, `timegm` | `<time.h>` | via `_gmtime64_s`, `_localtime64_s`, `_mkgmtime64`; `time_t` is 32 bits on i386 |
| `timespec_get` with `TIME_UTC` (C11), `timespec_getres` (C23) | `<time.h>` | `GetSystemTimePreciseAsFileTime` (Windows 8+), else `GetSystemTimeAsFileTime`; resolution reported as 100 ns |
| `roundeven`, `nextup`, `nextdown` | `<math.h>` | with `f` and `l` variants |
| `fmaximum`, `fminimum`, `fmaximum_num`, `fminimum_num`, `fmaximum_mag`, `fminimum_mag`, `fmaximum_mag_num`, `fminimum_mag_num` | `<math.h>` | with `f` and `l` variants; `long double` is `double` on these targets |

The `__STDC_VERSION_*_H__` macros are still not defined, since the headers
remain incomplete.

## Library: gaps

These are not declared and not available:

| Function(s) | Standard |
|---|---|
| `free_sized`, `free_aligned_sized` | C23; not provided together with `aligned_alloc` (below) |
| `aligned_alloc` | C11; memory from msvcrt's `_aligned_malloc` cannot be released with `free`, so a compatible version needs its own allocator |
| other new `<math.h>` functions (`compoundn`, `rootn`, `pown`, `sinpi`, ...) | C23 |
| `%b` in the `printf` family | C23; compiles, but msvcrt's `printf` does not implement it (not verified at run time) |
