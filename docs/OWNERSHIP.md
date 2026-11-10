# Ownership checking

tcc-win can check, at compile time, that a heap resource has exactly one
owner and is released exactly once. The feature is opt-in per variable and
does not change the generated code.

## Declaring owners

```c
[[owner]] char *p = malloc(n);            // C23 attribute
[[tcc::owner]] FILE *f = fopen(...);      // same, with vendor prefix
__attribute__((owner)) char *q;           // GNU spelling
void release([[owner]] char *p);          // function takes ownership
[[owner]] char *make(void);               // documents the result (not checked at the caller)
```

Only **local variables and parameters of pointer type** are tracked. The
macro `__TCC_OWNERSHIP__` and `__has_c_attribute(owner)` are defined when the
feature is available.

In the Windows headers shipped with tcc-win, `free()` is declared as taking
an owner, so `free(p)` releases `p`.

## Rules

Each owner is in one of the states *owned*, *moved* (also: uninitialized),
*null*, or *maybe moved*.

**Moves.** Ownership is transferred when the plain variable name is used as

- initializer of, or right-hand side of an assignment to, another owner:
  `[[owner]] char *b = a;`
- argument for a parameter declared `[[owner]]`: `free(a);`
- operand of `return`: `return a;`

Every other use (`a[0]`, `a->x`, `f(a)` for a non-owner parameter,
`char *view = a;`) is a *borrow*. Borrows are **not** checked: there is no
lifetime analysis and no shared/mutable exclusivity as in Rust.

**Errors reported**

| Diagnostic | Meaning |
|---|---|
| `use of owner 'p' after move (or before initialization)` | use-after-free, double free, use of uninitialized owner |
| `use of owner 'p' which was moved on some paths` | as above, on some control-flow paths |
| `assignment overwrites owner 'p' which still owns a resource (leak)` | old value was neither released nor moved |
| `owner 'p' still owns a resource at 'return' / at end of scope (leak)` | missing release |
| `owner 'p' possibly still owns a resource at ...` | released on some paths only |
| `owner 'p' is moved inside a loop` | would be moved again in the next iteration |

**Null.** An owner initialized or assigned with `nullptr`/`0`, and an owner
inside `if (!p) { ... }` (or in the `else` part of `if (p)`), is *null* and
need not be released. Only these two test forms are recognised.

**Control flow.** States are merged at the end of `if`/`else`, `switch`,
and loops, taking `return` and `break` into account.

## Limitations

The checker runs inside TinyCC's single-pass parser, so it is deliberately
simple. It is a lint for common mistakes, **not a soundness guarantee**:

- `goto` and `continue` paths are not tracked.
- Taking the address (`&p`) marks `p` as owned, on all paths.
- Owners in structs, globals, statics and arrays are not tracked.
- Aliases created by borrows can dangle without a diagnostic.
- Conditions other than `if (p)` / `if (!p)` do not narrow to *null*
  (e.g. `if (p == NULL)`, `while (p)`), and `[[owner]] T *p = NULL;` in a
  declaration counts as *owned* (use `nullptr` or `0`).
- Return values of `[[owner]]` functions may be discarded silently.
- Infinite loops (`for (;;)`) are treated like loops that may run zero times.
- Compilation stops at the first ownership error (as for all TinyCC errors).
- At most 255 owners per function.
