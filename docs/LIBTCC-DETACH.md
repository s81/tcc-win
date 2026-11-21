# Keeping compiled code after `tcc_delete` (`tcc_detach`)

With libtcc you compile C source in memory and call it: `tcc_new`,
`tcc_compile_string`, `tcc_relocate`, `tcc_get_symbol`. The compiled code
belongs to the `TCCState`, so it lives as long as the whole compiler state:
its sections, symbol tables, include paths and so on.

`tcc_detach` moves the code out of the state into a small `TCCCode` object.
You can then delete the state at once and keep only the code. This suits
programs that compile many small pieces of code and keep them for a long
time: plugins, expression evaluators, rule engines, JIT-style caches.

## API

Declared in `libtcc.h`:

```c
typedef struct TCCCode TCCCode;

TCCCode *tcc_detach(TCCState *s);
void    *tcc_code_symbol(TCCCode *code, const char *name);
void     tcc_code_free(TCCCode *code);
```

| Function | What it does |
|---|---|
| `tcc_detach(s)` | Call after a successful `tcc_relocate(s)`. Moves the run image, the global symbol addresses and the DLLs loaded for the code into a new `TCCCode`. Returns `NULL` and reports an error through the state's error function if `tcc_relocate` has not run, or if the state was set up with `-bt` or `-b` (backtraces and bounds checking need the state at run time). |
| `tcc_code_symbol(code, name)` | Address of a global function or variable in the code, or `NULL` if there is none. Names are written as in C; a leading underscore is added on targets that need it, as `tcc_get_symbol` does. Static (file-local) symbols cannot be found. |
| `tcc_code_free(code)` | Unloads the code: releases its memory, unregisters its unwind data (x86_64) and unloads the DLLs it loaded. `NULL` is allowed. |

After `tcc_detach` the state no longer has code: `tcc_get_symbol(s, ...)`
returns `NULL` and `tcc_delete(s)` does not touch the detached code. Delete
the state after detaching it.

## Example

```c
#include <stdio.h>
#include <libtcc.h>

/* compile C source to a function that stays usable after its TCCState is gone */
static TCCCode *build(const char *src)
{
    TCCState *s = tcc_new();
    TCCCode *code = NULL;
    if (!s)
        return NULL;
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    if (tcc_compile_string(s, src) == 0 && tcc_relocate(s) == 0)
        code = tcc_detach(s);        /* the code moves out of the state */
    tcc_delete(s);                   /* frees the compiler, keeps the code */
    return code;
}

int main(void)
{
    TCCCode *sq = build("int f(int x) { return x * x; }");
    TCCCode *cu = build("int f(int x) { return x * x * x; }");
    int (*square)(int), (*cube)(int);

    if (!sq || !cu)
        return 1;
    square = (int (*)(int))tcc_code_symbol(sq, "f");
    cube   = (int (*)(int))tcc_code_symbol(cu, "f");
    printf("%d %d\n", square(7), cube(3));   /* 49 27 */

    tcc_code_free(sq);                       /* unload one, keep the other */
    printf("%d\n", cube(4));                 /* 64 */
    tcc_code_free(cu);
    return 0;
}
```

Build it from the `win32` folder of a tcc-win build (or its installed copy),
and run it with `libtcc.dll` on the `PATH` or next to the program:

```
tcc -Ilibtcc plugin.c -Llibtcc -ltcc -o plugin.exe
```

Each `TCCCode` is independent: both pieces define `f`, and freeing one does
not affect the other.

## Rules

- Do not call into a `TCCCode`, or use a pointer into its data, after
  `tcc_code_free`. The memory is released, so the call faults.
- Pointers the compiled code returns into its own static data (string
  literals, globals) are valid until `tcc_code_free`, not until
  `tcc_delete`.
- Code from one state that calls functions of another must not outlive it;
  libtcc does not track such references.
- The code keeps the DLLs it loaded (for example a library added with
  `tcc_add_library`) until `tcc_code_free`.

## Memory and protection of in-memory code on Windows

These apply to all in-memory code (`TCC_OUTPUT_MEMORY`, `tcc -run`),
detached or not:

- **W^X.** Code pages are never writable: code is execute-read, read-only
  data is read-only, and other data is read-write. Code that writes to its
  own instructions faults.
- **Release.** The run image is released with `VirtualFree` by `tcc_delete`
  (or by `tcc_code_free` after a detach). Compiling and unloading the same
  code 2000 times does not grow the process's memory
  (`tests/libtcc_test_detach.c` checks this).
- **Thread-local variables.** `_Thread_local` and `__declspec(thread)`
  work: each thread gets its own copy, made from the initial values the first
  time the thread touches one, and freed when the thread exits or when the
  code is unloaded (`tcc_delete` or `tcc_code_free`). Each compiled image
  with thread-local variables uses one Win32 TLS slot, which must be below 64
  (otherwise compiling fails with `no free Win32 TLS slot for thread-local
  variables in memory`), and one FLS slot. Code that switches fibers must not
  use thread-local variables.
- **2 GiB limit.** An in-memory image larger than 2 GiB is an error, as is a
  single section over 2 GiB in an in-memory image or a PE file. The message
  names the section, for example
  `section '.bss' would exceed 2 GiB`, or
  `in-memory image of 3000 MiB exceeds 2 GiB`.

## Tests

`tests/libtcc_test_detach.c` covers detaching, symbol lookup, unloading, page
protection, memory growth over repeated cycles, `-bt` being refused, the
2 GiB errors and thread-local variables used from several threads. The upstream test suite builds and runs it
(`make libtest_detach` in `tests/`). To run it by hand from PowerShell in the
repository root:

```powershell
cd win32
.\tcc.exe -B. -I.. -I..\include ..\tests\libtcc_test_detach.c -L. -ltcc -o $env:TEMP\detach.exe
& $env:TEMP\detach.exe "-B$PWD" "-I$PWD\..\include" "-I$PWD\.."
```

Give the test absolute paths, as above: with `-B.` its compiles do not find
`tccdefs.h`. It prints `libtcc_test_detach: all checks passed` and exits
with 0.
