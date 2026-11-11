# tcc-win

A Windows-only fork of the [Tiny C Compiler](https://repo.or.cz/tinycc.git)
(TinyCC `mob`, commit `aade7baf`, 2026-11-09) with three changes:

1. **C23 by default** (`__STDC_VERSION__ == 202311L`).
2. **Rust-like ownership and borrow checking** with non-lexical lifetimes
   for pointers marked `[[owner]]`, `[[borrow]]` and `[[borrow_mut]]`
   (opt-in, compile-time only, see [docs/OWNERSHIP.md](docs/OWNERSHIP.md)),
   including `[[owner]]` fields of local structs.
3. **No `goto`** (see [below](#no-goto)).

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

## Download

Each release on the GitHub *Releases* page has a zip per target
(`tcc-win-<version>-<target>-win32.zip`) and a `SHA256SUMS` file. Unzip it
anywhere and run `tcc-win\tcc.exe`; it finds its headers and libraries next
to itself. The binaries are built with MSVC and tested (compile, link and
run) before they are published.

To publish a release, open *Actions → release → Run workflow* on GitHub,
keep the branch `main`, enter a version such as `v0.1.0` and run it: the
workflow creates the tag, builds and tests the zips, and publishes the
release. Pushing a tag `v*` from a clone does the same.

## Building

On Windows (needs an existing C compiler: gcc, cl or tcc):

```bat
cd win32
build-tcc.bat -c gcc -t x86_64
```

tcc-win can also build itself. Copy a built `win32` directory elsewhere
(for example to `C:\tcc`), then rebuild with it:

```bat
cd win32
build-tcc.bat -clean
build-tcc.bat -c C:\tcc\tcc.exe -t x86_64
```

CI bootstraps x86_64, i386 and arm64 this way in three stages (MSVC, then tcc-win
twice) and requires the last two stages to be byte-identical.

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

[docs/C23-STATUS.md](docs/C23-STATUS.md) lists what an audit against C23
found supported and what is still missing.

| Feature | Status |
|---|---|
| `__STDC_VERSION__ 202311L`, `-std=c23/c2x/gnu23` | added (default) |
| `bool`, `true`, `false`, `static_assert`, `alignas`, `alignof`, `thread_local` without headers | added (predefined macros, as permitted by C23 6.4.1) |
| `static_assert` without message | upstream |
| `nullptr`, `nullptr_t` | added (`nullptr_t` is a distinct type) |
| `constexpr` | added: integer and floating objects are true constants (no address); members and elements of `constexpr` structs, unions and arrays are constant expressions (the object is static and read-only) |
| `auto` type inference | added (single plain declarator: `auto x = expr;`) |
| `typeof`, `typeof_unqual` | upstream / added |
| `_BitInt(N)`, `wb`/`uwb` constants | added for N <= 64 (`BITINT_MAXWIDTH` is 64, the minimum C23 requires) |
| `[[attributes]]`: `noreturn`, `deprecated`, `fallthrough`, `maybe_unused`, `nodiscard`, `unsequenced`, `reproducible` | added: parsed in all positions; `noreturn`, `nodiscard` and `deprecated` (warnings, see below) and `owner`, `borrow`, `borrow_mut` have an effect |
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

- `_BitInt(N)` with N > 64 is rejected (`BITINT_MAXWIDTH` stays 64): the
  single-pass code generator keeps every value in one or two machine
  registers, and wide integers would need a separate memory-based value
  class for every operator, conversion and ABI. The result of `c ? a : b` with
  `_BitInt` operands has the underlying standard type, and `_BitInt`
  bit-fields are not supported.
- `[[nodiscard]]` warns when the result of a direct call to a `[[nodiscard]]`
  function (or to a function returning a `struct`/`union`/`enum` declared
  `[[nodiscard]]`) is discarded in an expression statement; `(void)` casts
  silence it and the reason string is not printed. `[[deprecated("msg")]]`
  warns on use of a function, variable or typedef (not on struct tags,
  enumerators or members). Both are ordinary warnings, enabled by default and
  silenced by `-w`.
- Decimal floating point (`_Decimal32/64/128`) is not supported; it is an
  optional feature in C23.
- The C library is the one of Windows (msvcrt). Library additions of C23
  such as `printf("%b")`, `memset_explicit` or `strdup` depend on it.
- Old-style (K&R) function definitions are an error in C23 mode
  (`-std=c23`, the default); they are accepted with `-std=c17` or lower. The
  upstream test suite (`tests/tcctest.c`) is compiled with `-std=c17`.

## No goto

`goto`, computed `goto *p` (GNU) and `asm goto` are compile errors:

```
error: 'goto' is not supported by tcc-win: use structured control flow (or -fgoto for legacy code)
```

This applies with every `-std=` setting, so tcc-win does not accept all
conforming C programs. `-fgoto` accepts `goto` again for existing code; with it,
the ownership checker does not follow `goto` paths. Without `-fgoto` the macro
`__TCC_NO_GOTO__` is defined. Labels themselves are still allowed.

tcc-win's own source, its runtime library and the upstream tests use `goto`,
so the build passes `-fgoto` whenever tcc-win compiles them (`Makefile`,
`lib/Makefile`, `win32/build-tcc.bat`).

## Tests

```bash
tests/tccwin-tests.sh
```

runs `tests/c23/` and `tests/ownership/` with the cross compiler (compile and
link only; the CI executes them on Windows). With `RUN=1` and a native `TCC=...` it also executes the programs.
The upstream test suite runs with `make test` after a native build, and on
Windows with `tests/test-win32.bat` (needs `make`, `sh` and gcc or clang as
reference compiler). CI runs it on x86_64, i386 and arm64 Windows.

## Licence

TinyCC is distributed under the GNU Lesser General Public License 2.1
(see [COPYING](COPYING)); the modifications in this repository are released
under the same licence.
