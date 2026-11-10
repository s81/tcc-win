# Ownership and borrow checking

tcc-win can check, at compile time, that a heap resource has exactly one
owner, is released exactly once, and is not changed or released while
someone else holds a pointer to it. The feature is opt-in per variable and
does not change the generated code.

```c
#include <stdlib.h>
#include <string.h>

void upcase([[borrow_mut]] char *s);           // may modify, may not keep or free

int main(void)
{
    [[owner]] char *p = malloc(16);
    if (!p)
        return 1;
    strcpy(p, "hello");
    {
        [[borrow]] const char *view = p;        // shared borrow until '}'
        upcase(p);  // error: cannot pass 'p' as mutable borrow while it is borrowed by 'view'
        free(p);    // error: cannot move owner 'p' while it is borrowed by 'view'
    }
    free(p);        // ok: nothing borrows p any more
    return 0;
}
```

## Declaring owners and borrows

```c
[[owner]] char *p = malloc(n);            // owns the resource
[[borrow]] const char *v = p;             // shared borrow: read-only, many at a time
[[borrow_mut]] char *m = p;               // mutable borrow: exclusive

void release([[owner]] char *p);          // function takes ownership
size_t len([[borrow]] const char *s);     // function borrows for the call
void edit([[borrow_mut]] char *s);        // function borrows mutably for the call
[[owner]] char *make(void);               // documents the result (not checked at the caller)
```

Each attribute can also be written with the vendor prefix (`[[tcc::owner]]`,
`[[tcc::borrow]]`, `[[tcc::borrow_mut]]`) or in GNU spelling
(`__attribute__((owner))`, `__attribute__((borrow))`,
`__attribute__((borrow_mut))`), which also works with `-std=c11`.

Only **local variables and parameters of pointer type** are tracked. A
`[[borrow]]` must point to a `const` type, so that C itself rejects writes
through it. The macro `__TCC_OWNERSHIP__` and `__has_c_attribute(owner)`,
`__has_c_attribute(borrow)`, `__has_c_attribute(borrow_mut)` are defined
when the feature is available.

In the Windows headers shipped with tcc-win, `free()` is declared as taking
an owner, so `free(p)` releases `p`.

## Owners

Each owner is in one of the states *owned*, *moved* (also: uninitialized),
*null*, or *maybe moved*.

**Moves.** Ownership is transferred when the plain variable name is used as

- initializer of, or right-hand side of an assignment to, another owner:
  `[[owner]] char *b = a;`
- argument for a parameter declared `[[owner]]`: `free(a);`
- operand of `return`: `return a;`

**Null.** An owner initialized or assigned with `nullptr`/`0`, and an owner
inside `if (!p) { ... }` (or in the `else` part of `if (p)`), is *null* and
need not be released. Only these two test forms are recognised.

## Borrows

A borrow is created when the plain name of an owner or of another borrow is
used as

- initializer of, or right-hand side of an assignment to, a `[[borrow]]` or
  `[[borrow_mut]]` variable: `[[borrow]] const char *v = p;`
- argument for a `[[borrow]]` or `[[borrow_mut]]` parameter: `len(p)`.

**Lifetimes are lexical.** A borrow held in a variable lasts until the end of
the block (`{ ... }`) that declares the variable; assigning the variable
again ends the old borrow and starts a new one. A borrow passed as an
argument lasts for the call. Put borrows in an inner block to end them early:

```c
[[owner]] char *p = malloc(16);
{
    [[borrow]] const char *v = p;
    puts(v);
}               // v ends here
free(p);        // ok
```

Unlike Rust 2018 and later (non-lexical lifetimes), a borrow does not end at
its last use, because TinyCC compiles in a single pass and cannot see later
uses.

**Rules.** While a variable is borrowed:

- it cannot be moved (`free(p)`, `return p`, `[[owner]] q = p`), assigned,
  or have its address taken;
- if the borrow is mutable, the variable cannot be used at all, and no other
  borrow of it can be created;
- if the borrow is shared, further shared borrows are allowed, but no
  mutable borrow.

Borrows of borrows are tracked the same way: a mutable borrow can be
reborrowed (the original cannot be used while the reborrow lives), a shared
borrow can be copied, and a shared borrow can never become a mutable one.

A borrow can never be moved (`free(v)` on a borrow is an error) and must not
outlive what it borrows from. A function may return a borrow parameter, but
not a borrow of one of its own owners (including `[[owner]]` parameters,
which the function must release).

Within one call, the same variable may be passed to several `[[borrow]]`
parameters, but not to a `[[borrow_mut]]` parameter and any other borrow
parameter at the same time (aliasing).

## Errors reported

| Diagnostic | Meaning |
|---|---|
| `use of owner 'p' after move (or before initialization)` | use-after-free, double free, use of uninitialized owner |
| `use of owner 'p' which was moved on some paths` | as above, on some control-flow paths |
| `assignment overwrites owner 'p' which still owns a resource (leak)` | old value was neither released nor moved |
| `owner 'p' still owns a resource at 'return' / at end of scope (leak)` | missing release |
| `owner 'p' possibly still owns a resource at ...` | released on some paths only |
| `owner 'p' is moved inside a loop` | would be moved again in the next iteration |
| `cannot move owner 'p' while it is borrowed by 'v'` | release or transfer while a pointer to it is held |
| `cannot move owner 'p' while it is borrowed by an argument of the same call` | e.g. `consume(view(p), p)` style aliasing |
| `cannot assign to 'p' while it is borrowed by 'v'` | the borrow would see a different object |
| `cannot take the address of 'p' while it is borrowed by 'v'` | `&p` could be used to change `p` |
| `cannot use 'p' while it is mutably borrowed by 'm'` | exclusive access of a mutable borrow |
| `cannot borrow 'p' as mutable/shared because it is also borrowed as ... by 'x'` | conflicting borrows |
| `cannot borrow 's' as mutable: it is a shared borrow` | shared borrows are read-only |
| `cannot pass 'p' as mutable borrow while it is borrowed by 'v'` | conflicting borrow at a call |
| `cannot pass shared borrow 's' as mutable borrow` | shared borrows are read-only |
| `'a' and 'b' are passed to the same call and one of them is a mutable borrow (aliasing)` | two arguments refer to the same object |
| `cannot move out of borrow 'v'` | a borrow does not own the resource |
| `cannot return borrow 'v' of local owner 'p'` | the result would dangle |
| `'w' does not live long enough: it is still borrowed by 'v'` | the borrow outlives its source |
| `use of borrow 'v' before initialization` / `... not initialized on all paths` | uninitialized borrow |
| `[[borrow]] requires a pointer to const` | use `[[borrow_mut]]` for write access |

**Control flow.** States are merged at the end of `if`/`else`, `switch`,
and loops, taking `return` and `break` into account. A borrow assigned on
different paths borrows from all of the possible sources.

## Limitations

The checker runs inside TinyCC's single-pass parser, so it is deliberately
simple. It is a lint for common mistakes, **not a soundness guarantee**:

- Only plain variable names create moves and borrows. Expressions such as
  `p + 1`, `strchr(p, 'x')` or `(char *)s` are not tracked, and a borrow
  initialized from them is not connected to any owner.
- Plain (unannotated) pointers are never checked, so `char *alias = p;` can
  still dangle.
- With a shared borrow alive, writes through the owner itself (`p[0] = 1`)
  are not detected; only C's `const` protects writes through the borrow.
- Lifetimes are lexical (see above).
- `goto` and `continue` paths are not tracked.
- Taking the address (`&p`) marks `p` as owned, on all paths.
- Owners and borrows in structs, globals, statics and arrays are not tracked.
- Conditions other than `if (p)` / `if (!p)` do not narrow to *null*
  (e.g. `if (p == NULL)`, `while (p)`), and `[[owner]] T *p = NULL;` in a
  declaration counts as *owned* (use `nullptr` or `0`).
- Return values of `[[owner]]` functions may be discarded silently.
- Infinite loops (`for (;;)`) are treated like loops that may run zero times.
- Compilation stops at the first ownership error (as for all TinyCC errors).
- At most 64 owners and borrows per function.
