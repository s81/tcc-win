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
| `nullptr`, `nullptr_t` | added (`nullptr` has type `void *`, not a distinct `nullptr_t`) |
| `constexpr` | added: integer objects are true constants (no address); other types are treated as `const` |
| `auto` type inference | added (single plain declarator: `auto x = expr;`) |
| `typeof`, `typeof_unqual` | upstream / added |
| `[[attributes]]`: `noreturn`, `deprecated`, `fallthrough`, `maybe_unused`, `nodiscard`, `unsequenced`, `reproducible` | added: parsed; only `noreturn` (and `owner`) have an effect |
| `__has_c_attribute`, `__has_include`, `__has_embed` | added / upstream / added |
| `#embed` | added (without `limit`/`prefix`/`suffix`/`if_empty` parameters) |
| `#elifdef`, `#elifndef`, `#warning` | added / added / upstream |
| binary literals `0b101`, digit separators `1'000'000` | upstream / added |
| `u8'c'` character constants | added (type `int`, not `char8_t`) |
| `enum E : type` | upstream |
| empty initializer `= {}` (also for scalars) | upstream / added |
| unnamed parameters in function definitions | added |
| declarations after labels | upstream |
| `unreachable()` in `<stddef.h>` | added |
| `_BitInt`, `wb` literals | **not supported** |
| `__VA_OPT__` | **not supported** |
| `va_start` with one argument, `f(...)` without named parameter | **not supported** |
| `<stdbit.h>`, `<stdckdint.h>`, `char8_t` | **not supported** |
| decimal floating point (optional in C23) | not supported |
| storage-class specifiers in compound literals | not supported |

## Tests

```bash
tests/tccwin-tests.sh
```

runs `tests/c23/` and `tests/ownership/` with the cross compiler (compile and
link only). With `RUN=1` and a native `TCC=...` it also executes the programs.
The upstream test-suite is still available in `tests/` (`make test` after a
native build).

## Licence

TinyCC is distributed under the GNU Lesser General Public License 2.1
(see [COPYING](COPYING)); the modifications in this repository are released
under the same licence.
