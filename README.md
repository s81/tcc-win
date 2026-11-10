# tcc-win

A Windows-only fork of the [Tiny C Compiler](https://repo.or.cz/tinycc.git)
(TinyCC `mob`, commit `aade7baf`, 2026-11-09) with two additions:

1. **C23 by default** (`__STDC_VERSION__ == 202311L`).
2. **Rust-like ownership checking** for pointers marked `[[owner]]`
   (opt-in, compile-time only, see [docs/OWNERSHIP.md](docs/OWNERSHIP.md)).

```c
#include <stdlib.h>

int main(void)
{
    [[owner]] char *p = malloc(16);
    if (!p)
        return 1;
    free(p);        // free() takes ownership: p is moved
    return p[0];    // error: use of owner 'p' after move
}
```

## Targets

Only Windows PE output is supported: `x86_64-win32`, `i386-win32` and
`arm64-win32`. The ARM (32 bit), RISC-V, C67, IL, COFF and Mach-O backends
of upstream were removed.

## Building

On Windows (needs an existing C compiler: gcc, cl or tcc):

```bat
cd win32
build-tcc.bat -c gcc -t x86_64
```

Cross compilers hosted on Linux:

```bash
./configure
```

```bash
make cross-x86_64-win32 cross-i386-win32 cross-arm64-win32
```

```bash
./x86_64-win32-tcc -Bwin32 -Iinclude -L. hello.c -o hello.exe
```

## C23 support

Use `-std=c11` or `-std=c99` to get the previous behaviour.

| Feature | Status |
|---|---|
| `__STDC_VERSION__ 202311L`, `-std=c23/c2x/gnu23` | added (default) |
| `bool`, `true`, `false`, `static_assert`, `alignas`, `alignof`, `thread_local` without headers | added (predefined macros, as permitted by C23 6.4.1) |
| `static_assert` without message | upstream |
| `nullptr`, `nullptr_t` | added (`nullptr_t` is a distinct type) |
| `constexpr` | added: integer and floating objects are true constants (no address); other types are treated as `const` |
| `auto` type inference | added (single plain declarator: `auto x = expr;`) |
| `typeof`, `typeof_unqual` | upstream / added |
| `_BitInt(N)`, `wb`/`uwb` constants | added for N <= 64 (`BITINT_MAXWIDTH` is 64, the minimum C23 requires) |
| `[[attributes]]`: `noreturn`, `deprecated`, `fallthrough`, `maybe_unused`, `nodiscard`, `unsequenced`, `reproducible` | added: parsed in all positions; only `noreturn` (and `owner`) have an effect |
| `__has_c_attribute`, `__has_include`, `__has_embed` | added / upstream / added |
| `#embed` with `limit`, `prefix`, `suffix`, `if_empty` | added (`limit` takes an integer constant, not an expression) |
| `#elifdef`, `#elifndef`, `#warning` | added / added / upstream |
| `__VA_OPT__` (also with `#` and `##`) | added |
| `va_start(ap)` with one argument, `f(...)` without named parameter | added |
| `f()` means `f(void)` | added |
| binary literals `0b101`, digit separators `1'000'000` | upstream / added |
| `u8'c'` character constants, `u8"..."` strings, `char8_t` | added (the constant has type `int` and the string type `char[]`, not `char8_t`) |
| `enum E : type` | upstream |
| empty initializer `= {}` (also for scalars) | upstream / added |
| unnamed parameters in function definitions | added |
| storage-class specifiers in compound literals | added (`static`, `constexpr`) |
| declarations after labels | upstream |
| `unreachable()` in `<stddef.h>` | added |
| `<stdbit.h>`, `<stdckdint.h>` | added |
| `BITINT_MAXWIDTH`, `*_WIDTH` in `<limits.h>` | added |

Known deviations and gaps:

- `_BitInt(N)` with N > 64 is rejected. The result of `c ? a : b` with
  `_BitInt` operands has the underlying standard type, and `_BitInt`
  bit-fields are not supported.
- `[[nodiscard]]` and `[[deprecated]]` produce no diagnostics.
- Decimal floating point (`_Decimal32/64/128`) is not supported; it is an
  optional feature in C23.
- The C library is the one of Windows (msvcrt). Library additions of C23
  such as `printf("%b")`, `memset_explicit` or `strdup` depend on it.
- Old-style (K&R) function definitions are still accepted.

## Tests

```bash
tests/tccwin-tests.sh
```

runs `tests/c23/` and `tests/ownership/` with the cross compiler (compile and
link only; the CI executes them on Windows). With `RUN=1` and a native `TCC=...` it also executes the programs.
The upstream test-suite is still available in `tests/` (`make test` after a
native build).

## Licence

TinyCC is distributed under the GNU Lesser General Public License 2.1
(see [COPYING](COPYING)); the modifications in this repository are released
under the same licence.
