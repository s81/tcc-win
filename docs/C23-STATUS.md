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
| `[[deprecated]]`, `[[fallthrough]]`, `[[maybe_unused]]`, `[[nodiscard]]`, `[[noreturn]]`, `[[unsequenced]]`, `[[reproducible]]` | also `[[__name__]]` spellings, `__has_c_attribute`; `nodiscard` and `deprecated("msg")` give warnings (default on, `-w` silences) |
| empty initializer `= {}` | |
| enums with a fixed underlying type, enumerators wider than `int` | |
| unnamed parameters in function definitions | |
| labels before declarations | |
| `static` / `constexpr` compound literals | |
| `f()` means `f(void)`; `f(...)`; `va_start(ap)` | old-style (K&R) definitions are an error (accepted with `-std=c17` or lower) |
| binary literals, digit separators `'` | |
| `u8` character constants | type `unsigned char`; one UTF-8 code unit |
| `_BitInt(N)`, `wb`/`uwb` suffixes | `BITINT_MAXWIDTH` is 64, the minimum C23 requires; N > 64 is not feasible in the single-pass code generator |
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
| `FLT_SNAN`, `DBL_SNAN`, `LDBL_SNAN` in `<float.h>` | constant expressions from the builtins `__builtin_nans`, `__builtin_nansf`, `__builtin_nansl` (also `__builtin_nan*`, `__builtin_inf*`) |
| `__STDC_VERSION_*_H__` = `202311L` | defined only in the headers that are complete: `<assert.h>`, `<float.h>`, `<limits.h>`, `<stdarg.h>`, `<stdbit.h>`, `<stdckdint.h>`, `<stddef.h>`, `<stdint.h>`, `<math.h>` (Windows targets) |

## Language: gaps

None found by this audit.

## Headers: gaps

| Gap | Observed |
|---|---|
| `__STDC_VERSION_*_H__` of `<stdlib.h>`, `<string.h>`, `<stdio.h>`, `<time.h>`, `<uchar.h>`, `<wchar.h>`, `<inttypes.h>`, `<setjmp.h>`, `<fenv.h>`, `<complex.h>`, `<tgmath.h>`, `<stdatomic.h>` | deliberately not defined: the macro states that the header is C23-complete, and these lack the library functions below (or were not audited) |

## Library: supported

tcc-win links against `msvcrt.dll`, the Windows system C runtime, which
predates C11/C23. The following functions are provided by tcc-win's runtime
library (`libtcc1.a`, sources `lib/c23rt.c` and `lib/c23math.c`) and declared in the shipped
headers when `__STDC_VERSION__` is at least `202311L` (`201112L` for
`timespec_get` and `TIME_UTC`). Tested by `tests/c23/c23_runtime.c` and
`tests/c23/c23_math2.c`, `tests/c23/c23_math3.c` and `tests/c23/c23_aligned.c`.

| Function(s) | Header | Notes |
|---|---|---|
| `memset_explicit`, `strndup` | `<string.h>` | `strdup` comes from msvcrt |
| `memalignment` | `<stdlib.h>` | |
| `aligned_alloc` (C11), `free_sized`, `free_aligned_sized` | `<stdlib.h>` | blocks are released by `free`/`realloc`, which C23 mode binds to wrappers; see [`aligned_alloc` and `free`](#aligned_alloc-and-free) |
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
| `llogb`, `FP_LLOGB0`, `FP_LLOGBNAN` | `<math.h>` | `long` is 32 bits: both macros are `LONG_MIN`, like `FP_ILOGB0`/`FP_ILOGBNAN` (`INT_MIN`) |
| `totalorder`, `totalordermag`, `getpayload`, `setpayload`, `setpayloadsig`, `iseqsig` | `<math.h>` | operands read through pointers as bits, so signaling NaNs survive on i386; `getpayload` of a non-NaN is -1 |
| `fadd`, `fsub`, `fmul`, `fdiv`, `ffma`, `fsqrt`, `faddl` ... `fsqrtl`, `daddl`, `dsubl`, `dmull`, `ddivl`, `dfmal`, `dsqrtl` | `<math.h>` | correctly rounded (one rounding, to nearest; the dynamic rounding mode is ignored), computed exactly in integer arithmetic, subnormal and overflow results included |
| C99 `fma`, `remquo`, `remainder`, `nearbyint`, `erf`, `erfc` (each with `f` and `l`), `atan2l`, `copysignl` | `<math.h>` | missing from msvcrt.dll; `fma` correctly rounded, `remquo`/`remainder` exact, `erf` within 2 ulps and `erfc` within about 10 ulps (built on msvcrt's `exp`) |
| C99 `isgreater`, `isgreaterequal`, `isless`, `islessequal`, `islessgreater`, `isunordered`, `float_t`, `double_t`, `math_errhandling`, `MATH_ERRNO`, `MATH_ERREXCEPT` | `<math.h>` | were missing from the header; `math_errhandling` is `MATH_ERRNO` (msvcrt's functions set `errno`) |
| `rint`, `lrint`, `llrint`, `trunc` (arm64) | `<math.h>` | the inline versions used x87 assembly and did not compile for arm64 |

The `<math.h>` functions implemented in this library set neither `errno` nor
the floating-point exception flags (msvcrt's own functions set `errno`);
`fromfpx` behaves like `fromfp`.

### `aligned_alloc` and `free`

msvcrt has no `aligned_alloc`, and memory from its `_aligned_malloc` cannot
be released with `free`. tcc-win's `aligned_alloc` (`lib/c23rt.c`) therefore
works on top of `malloc`:

- an alignment up to what `malloc` already guarantees (8 on i386, 16 on
  x86_64 and arm64) returns plain `malloc` memory;
- a larger one over-allocates with `malloc` and returns an aligned pointer
  inside that block, storing the size and the original pointer in a header
  just before it. Such a pointer is recorded in a set of live aligned blocks
  (a hash table protected by a spin lock);
- a zero or non-power-of-two alignment returns `NULL`.

In C23 mode `<stdlib.h>` and `<malloc.h>` declare `free` and `realloc` with
asm labels, so calls (and their addresses, e.g. `fp = free`) bind to
`__tcc_free` and `__tcc_realloc` in `libtcc1.a`. These look a pointer up in
the set and pass anything not found there unchanged to msvcrt. The header is
never trusted on its own, so ordinary `malloc` pointers cannot be mistaken
for aligned blocks. The cost for them is one load and branch while no aligned
block is live, otherwise an alignment test and, for suitably aligned
pointers, a table lookup under the lock. `realloc` of an aligned block
returns ordinary `malloc` memory (C allows that; the alignment is not kept).
`free_sized` and `free_aligned_sized` ignore their size and alignment
arguments and call `__tcc_free`. `-D__TCC_MSVCRT_FREE` keeps msvcrt's
`free`/`realloc` and hides these three functions; `-b` (bounds checking) does the
same, because the bounds checker hooks `free`/`realloc` itself.

This was chosen over the alternatives because it is the only one that keeps
plain `free` correct: `_aligned_malloc` needs `_aligned_free`, and a
header tag alone (without the set) would read bytes that belong to the heap
before every ordinary pointer and could, however rarely, match them.

Not supported (heap corruption or a crash, as with `_aligned_malloc`):
a block with alignment above 8 (i386) / 16 (x86_64, arm64) released by
code that does not use the wrappers: code compiled with `-std=c11`/`c17` or
`-D__TCC_MSVCRT_FREE`, code that declares `free` itself without including
`<stdlib.h>`/`<malloc.h>`, other compilers' objects, msvcrt itself, or
another DLL/EXE (each module linked with `libtcc1.a` has its own set, so
aligned blocks must be freed in the module that allocated them). msvcrt's
`_msize`, `_expand`, `_recalloc` and `_aligned_free` do not accept them
either. In C11/C17 mode `aligned_alloc` is not declared.

### `printf` family: `%b` / `%B`

msvcrt's `printf` does not implement the C23 `%b`/`%B` conversions and prints
three exponent digits (`1.5e+003`). In C23 mode `<stdio.h>` therefore maps
`printf`, `fprintf`, `sprintf`, `snprintf`, `vprintf`, `vfprintf`, `vsprintf`
and `vsnprintf` to wrappers in `libtcc1.a` (`__tcc_printf` etc., source
`lib/c23printf.c`). The wrappers walk the format string and

- format `%b`/`%B` themselves, with the flags `#` (`0b`/`0B` prefix), `0`,
  `-`, width, precision and the length modifiers `hh h l ll j z t` and
  C23 `wN`/`wfN`;
- hand every other conversion to msvcrt's `_snprintf`, one specification at a
  time (with `*` resolved and integers passed as 64-bit values), and shorten
  the exponent of `e`/`E`/`g`/`G` output to the C form (at least two digits);
- write infinities and NaNs as `inf`/`nan` (`INF`/`NAN`) and handle `%n`;
- return the full length (C semantics) even when `snprintf` truncates.

The msvcrt-specific `I64`/`I32`/`I` length modifiers (used by `<inttypes.h>`)
are still accepted. Other output (`%p`, `%a`) is msvcrt's. Compile with
`-D__TCC_MSVCRT_PRINTF` to call msvcrt's functions directly. The wide
(`wprintf`) family and `scanf` are not wrapped. Tested by
`tests/c23/c23_printf_b.c`.

The `__STDC_VERSION_*_H__` macros of the other library headers are still
not defined, since those headers remain incomplete. `<math.h>` defines
`__STDC_VERSION_MATH_H__` (`202311L`): every C23 `<math.h>` function and
macro is available (the decimal floating-point ones are not required, as
`__STDC_IEC_60559_DFP__` is not defined).

## Library: gaps

None remain: `aligned_alloc`, `free_sized` and `free_aligned_sized`, the
last ones listed here, are now provided (with the limits described in
[`aligned_alloc` and `free`](#aligned_alloc-and-free)).
