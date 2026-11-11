# Ownership and borrow checking

tcc-win can check, at compile time, that a heap resource has exactly one
owner, is released exactly once, and is not changed or released while
someone else holds a pointer to it. The feature is opt-in per variable and
does not change the generated code.

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void upcase([[borrow_mut]] char *s);           // may modify, may not keep or free

int main(void)
{
    [[owner]] char *p = malloc(16);
    if (!p)
        return 1;
    strcpy(p, "hello");
    [[borrow]] const char *view = p;            // shared borrow of p
    upcase(p);      // error: cannot pass 'p' as mutable borrow while it is borrowed by 'view'
    puts(view);     // ... because view is used again here
    free(p);        // ok: view is not used any more
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

Only **local variables and parameters of pointer type** are tracked, plus
the elements of local arrays of owners (see *Arrays of owners*) and
`[[owner]]` struct members (see *Owners inside structs*). A
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

`*p = x` writes through `p`; it does not assign `p` (no overwrite check).

**Null.** An owner that is *null* need not be released. An owner is null
when it is initialized or assigned with a null constant (`nullptr`, `0`,
`NULL`, `(void *)0`), and where a **null test** shows it is null:

| Test | `p` is null |
|---|---|
| `if (!p)`, `if (p == NULL)` | in the `then` branch |
| `if (p)`, `if (p != NULL)` | in the `else` branch (and after the `if` if the `then` branch returns) |
| `while (p)`, `for (...; p != NULL; ...)` | after the loop, unless it was left by `break` |
| `while (!p)`, `while (p == NULL)` | inside the loop body |

A null test is one of `p`, `!p`, `p == N`, `p != N`, `N == p`, `N != p`, where
`N` is a null constant, optionally in parentheses or negated with `!`. A
condition that combines it with anything else (`p == NULL && n > 0`) does
not narrow. `do ... while (p)` does not narrow either.

```c
void list_free([[owner]] struct node *n)
{
    while (n != NULL) {
        [[owner]] struct node *next = n->next;
        free(n);
        n = next;
    }                       // n is null here: no leak
}
```

## Borrows

A borrow is created when the plain name of an owner or of another borrow is
used as

- initializer of, or right-hand side of an assignment to, a `[[borrow]]` or
  `[[borrow_mut]]` variable: `[[borrow]] const char *v = p;`
- argument for a `[[borrow]]` or `[[borrow_mut]]` parameter: `len(p)`.

**Lifetimes are non-lexical**, as in Rust 2018 and later: a borrow is live
from where it is created until its **last use**, not until the end of its
block. An operation that conflicts with a borrow (for example `free(p)` while
`v` borrows `p`) is an error only if the borrow is used again afterwards:

```c
[[owner]] char *p = malloc(16);
[[borrow]] const char *v = p;
puts(v);        // last use of v: the borrow ends here
free(p);        // ok
```

The details:

- **Loops.** A borrow that is used inside a loop (including its condition)
  and was created before the loop is live until the end of the loop, because
  the next iteration uses it again.
- **Reassignment** (`v = q;`) ends the old borrow, unless the assignment is
  inside an `if`/`else` branch, `switch`, loop body, or the right operand of
  `&&`, `||` or `?:` relative to the declaration of `v`. There it may not
  run, so it counts as a use of the old borrow.
- **Same statement.** A conflict in the same statement as the last use is
  an error, e.g. `printf("%s %s", m, p)` while `m` mutably borrows `p`.
- **Scope.** A borrow ends at the end of its block at the latest.
- **Arguments.** A borrow passed as an argument lasts for the call.

TinyCC compiles in a single pass, so it cannot see later uses when it meets
a conflicting operation. It records the conflict and decides when the live
range of the borrow is known (at the end of its block, or when it is
reassigned); the error is then reported at the line of the conflicting
operation.

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
be used after the owner it borrows from went out of scope. A copy of a
borrow (`w = v;`) borrows from the same owner, so it may outlive `v`. A function may return a borrow parameter, but
not a borrow of one of its own owners (including `[[owner]]` parameters,
which the function must release).

Within one call, the same variable may be passed to several `[[borrow]]`
parameters, but not to a `[[borrow_mut]]` parameter and any other borrow
parameter at the same time (aliasing).

## Errors reported

All ownership errors of a file are reported in one run; compilation goes
on after each of them and fails at the end. At most one error is reported
per variable, because later ones are usually consequences of the first
(after `free(p); free(p);`, a third `free(p)` is not reported again).
Other C errors still stop compilation at once, as usual in TinyCC.

Each error says where the variable got into the state it complains about:

```
f.c:6: error: use of owner 'p' after move (moved at line 5)
f.c:9: error: cannot move owner 'p' while it is borrowed by 'v' (borrowed at line 7)
f.c:7: error: owner 'p' still owns a resource at 'return' (leak): release or move it (acquired at line 4)
```

"Acquired" is where the owner got its current resource (its declaration,
an assignment, or `&p`); for an `[[owner]]` parameter it is "acquired as a
parameter". When an owner was moved or acquired on several paths, the
line of the last one in the source is given.

| Diagnostic | Meaning |
|---|---|
| `use of owner 'p' after move (or before initialization)` | use-after-free, double free, use of uninitialized owner |
| `use of owner 'p' which was moved on some paths` | as above, on some control-flow paths |
| `assignment overwrites owner 'p' which still owns a resource (leak)` | old value was neither released nor moved |
| `owner 'p' still owns a resource at 'return' / at end of scope (leak)` | missing release |
| `owner 'p' possibly still owns a resource at ...` | released on some paths only |
| `owner 'p' is moved inside a loop` | would be moved again in the next iteration |
| `owner 'p->f' still owns a resource at release of 'p' (leak)` | `free(p)` while a field of `*p` still owns what it got in this function |
| `owner field 'p->f' left moved when 'p' is moved` | `*p` is handed on (returned, moved) with a dangling field |
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
| `'q' does not live long enough: it is still borrowed by 'v'` | the borrow is used after the owner `q` went out of scope |
| `use of borrow 'v' before initialization` / `... not initialized on all paths` | uninitialized borrow |
| `[[borrow]] requires a pointer to const` | use `[[borrow_mut]]` for write access |

**Control flow.** States are merged at the end of `if`/`else`, `switch`,
and loops, taking `return`, `break` and `continue` into account. The state
at a `continue` reaches the next iteration, like the end of the loop body,
so an owner released before `continue` is "moved inside a loop" unless it
gets a new value first. `break` and `continue` leave the loop body, so
owners declared in it must be released or null there (like at `return`). A borrow assigned on
different paths borrows from all of the possible sources.

## Arrays of owners

A local array whose element type is a pointer, declared `[[owner]]`, with
a constant size of at most 16 elements, is tracked **element by
element**: each element is an owner named `v[0]`, `v[1]`, ... with all
the rules above (moves, null tests, leaks, use after move, overwrite,
loops and control flow).

```c
[[owner]] char *v[3] = { malloc(4), malloc(8) };  // v[0], v[1] owned, v[2] null
free(v[0]);
[[owner]] char *p = v[1];       // moves v[1]
v[1] = malloc(2);               // ok: v[1] was moved
free(p);
free(v[0]);                     // error: use of owner 'v[0]' after move
return 0;                       // error: owner 'v[1]' still owns a resource at 'return'
```

Only an access with an **integer constant index** (`v[2]`, also from a
macro) is tracked. Elements set by an initializer list are owned (or null
for a null constant, and a plain owner name is moved into them);
elements not mentioned in the list are null; without an initializer they
are uninitialized.

**Conservative rule.** Any other use of the array, i.e. an element access
with a **non-constant index** (`v[i]`, also `v[N]` with an enumeration
constant or `constexpr` name), the array itself as a value (`f(v)`,
`memset(v, 0, sizeof v)`, `char **pp = v`) or `&v`, may change any
element. From that point in the source on, **none of the elements of
that array is checked any more**, on all paths, including the leak check
at the end of its scope. This keeps the usual loops over the array
(`for (i = 0; i < n; ++i) free(v[i]);`) free of false errors, but they are
not verified. `sizeof v` and `sizeof v[i]` are not uses.

Arrays with more than 16 elements, variable-length arrays, multi-dimensional
arrays, `static` arrays, arrays of structs, `[[borrow]]` arrays and arrays
whose elements would bring the number of variables tracked in the
function above 48 are not tracked at all. An
array parameter (`[[owner]] char *v[]`) is a pointer in C, so
it is tracked as one owner of type `char **`.

## Owners inside structs

A struct member can be declared `[[owner]]` (it must have pointer type).
For each **local variable or by-value parameter** of such a struct type,
every owner member is tracked as a separate owner named `s.field`, with all
the rules above: moves, null tests, leaks, use after move, loops and
control flow.

```c
struct buf {
    [[owner]] char *data;
    size_t len;
};

static void buf_free(struct buf *b) { free(b->data); b->data = NULL; }

int demo(void)
{
    struct buf s = { malloc(16), 16 };
    if (!s.data)                    // null test: s.data is null in 'then'
        return -1;
    [[owner]] char *p = s.data;     // moves s.data into p
    s.data = malloc(32);            // ok: s.data was moved
    free(p);
    buf_free(&s);                   // '&s' hands the fields to buf_free
    return 0;
}

int bad(void)
{
    struct buf s = { malloc(16), 16 };
    free(s.data);
    return s.data[0];   // error: use of owner 's.data' after move
}
```

**Initialization.** With an initializer list (`= { ... }`, `= {}`,
designators), a field initialized with a null constant, or not mentioned
(zero-initialized), is *null*; a field initialized with any other value is
*owned*, and a plain owner name used as its value is moved
(`{ .data = p }`). With an initializer that is another struct variable
(`struct buf t = s;`), the fields of `t` take over the states of the fields
of `s`, which are moved. With any other initializer (e.g. a function call
`= make()`) all owner fields are *owned*. Without an initializer they are
uninitialized (*moved*), like plain owners.

**Fields.** `s.field` is used exactly like the name of a plain owner:
`free(s.data)` and `return s.data;` move it, `s.data = malloc(n)` is checked
for overwriting an owned value, `[[owner]] char *p = s.data;` moves it,
`if (s.data == NULL)` and the other null tests narrow it, `&s.data` makes it
owned, and borrows (`[[borrow]] const char *v = s.data;`, `len(s.data)`)
borrow from it.

**The whole struct.** A struct with owner fields is a resource itself:

| Operation | Effect on the owner fields of `s` |
|---|---|
| `return s;`, `f(s)` (passed by value), any other use of the value of `s` | moved (each must be owned or null: a moved field is a use after move) |
| `t = s;`, `struct buf t = s;` | moved into the fields of `t` (`t` gets their states) |
| `s = expr;` | overwrite check for every field, then owned (copied states if `expr` is a struct variable) |
| `&s` (e.g. `init(&s)`, `cleanup(&s)`, `memcpy(&s, ...)`) | handed to whoever gets the address: treated as *null*, i.e. released and no longer checked |

The convention for `&s` follows the typical C pattern of init and cleanup
functions that take a pointer to the struct: **passing `&s` hands the
responsibility for the fields to the callee**. The checker cannot see what
the callee does, so after `&s` the fields are neither reported as leaks nor
as used after move until they get a new value (`s.data = malloc(..)`,
`s = ...`).

A by-value parameter of a struct type with owner fields owns its fields on
entry ("acquired as a parameter"), like an `[[owner]]` parameter.

**Nested structs.** A struct member whose type has owner fields (directly
or in its own members) makes the outer struct a struct with owner fields
too. Its fields are tracked with their full name, up to four member names
(`s.f`, `s.a.f`, `s.a.b.f`, `s.a.b.c.f`); deeper fields are not tracked.
Everything above applies to them, and the whole-struct operations apply to
a struct member as well as to the variable, for the fields under it:

```c
struct conn { struct buf in, out; [[owner]] char *name; };

struct conn c = { .in = { malloc(4), 4 } };  // c.in.data owned, c.out.data and c.name null
free(c.in.data);                              // moves c.in.data
c.out = make_buf();                           // overwrite check for c.out.data, then owned
buf_free(&c.out);                             // '&c.out' hands over the fields under c.out
struct buf b = c.in;                          // moves the fields under c.in into b
```

`&c` hands over all fields of `c`, nested ones included; `return c;` and
passing `c` by value move all of them. A member of an anonymous struct or
union is named without it (`s.x`), as in C. Designators (`{ .in.data = p }`)
and nested initializer lists set the nested fields; a member initialized
with a struct variable (`{ .in = b }`) takes over its field states.

**Pointer parameters.** For a parameter that is a pointer to a struct with
owner fields (`void f(struct buf *b)`, without `[[owner]]`/`[[borrow]]`),
the fields of the pointed-to object are tracked inside the function as
`b->data` (and `b->in.data` for nested members). They belong to the
caller's object, so they start *owned* and are never reported as leaks.
The checks are:

- use after move: `free(b->data); b->data[0] = 0;` and double frees;
- overwrite: assigning a field that was acquired in the function
  (`b->data = malloc(1); b->data = malloc(2);`) or that is possibly
  owned. The first assignment of a field that the function has not touched
  yet is accepted: init functions write into uninitialized objects;
- **left moved**: the object outlives the function, so a field that is
  moved (released, returned, moved into another owner) must get a new
  value (e.g. `NULL`) before every `return` and at the end of the function:

```c
void buf_free(struct buf *b)
{
    free(b->data);
    b->data = NULL;      // without this: error: owner field 'b->data'
}                        // left moved at return: assign it (e.g. NULL) ...
```

Passing the pointer itself to a function (`free(b)`, `other(b)`, except to
a `[[borrow]]` parameter), assigning it (`b = ...`) or taking its address
hands all its fields over: they are not checked any more, so a destructor
that frees the object itself (`free(b->data); free(b);`) needs no `NULL`.
Null tests of the pointer (`if (!b) return;`) do not hand anything over.

**Through owner pointers.** The owner fields of the object an owner
points to are tracked too: for `[[owner]] struct node *n` (a local
variable or parameter), `n->data`, `n->next`, `n->in.data` are owners,
and so are, through an `[[owner]]` member that points to such a struct,
`n->next->data`, `s.head->data` (a member of a local struct variable) and
`b->link->data` (behind a pointer parameter), up to four member names in
all. They are tracked from their first use in the function, in an
*unknown* state: owned, but not acquired in the function (`n` may come
from `malloc()`, so its fields may be uninitialized). The checks are:

- use after move: `free(n->data); n->data[0] = 0;` and double frees;
- overwrite: assigning a field that got a resource in the function
  (`n->data = malloc(1); n->data = malloc(2);`). The first assignment of a
  field in the unknown state is accepted;
- **leak at release**: passing `n` to an `[[owner]] void *` parameter
  (`free(n)`) releases the object, but such a function cannot know its
  fields. A field that got a resource in the function and still owns it
  is an error; fields in the unknown state are not reported:

```c
[[owner]] struct node *n = malloc(sizeof *n);
if (!n)
    return;
n->next = NULL;
n->data = malloc(16);
free(n);    // error: owner 'n->data' still owns a resource at release of 'n' (leak)
```

- **left moved**: when `n` is moved anywhere else (`return n;`,
  `[[owner]] struct node *m = n;`, `node_free(n)` with an
  `[[owner]] struct node *` parameter), the object lives on, so a field
  that is moved must have got a new value first (as for pointer
  parameters at `return`);
- borrows: `free(n)` while a borrow of `n->data` is still used is
  "cannot move owner 'n' while it is borrowed".

Fields reached through an owner are never reported as leaks at the end
of a scope or at `return`: the owner itself is (`n` still owns a
resource), and whoever releases it is responsible for its fields.

Everything that the checker cannot follow makes these fields *unknown*
again, as on their first use (owned, not acquired in the function):

- `n` gets a new value or is moved (the fields are those of another
  object);
- any use of the value of `n` other than `n->...`, e.g. `struct node *q
  = n;`, `init(n)` (an unannotated or `[[borrow_mut]]` parameter),
  `printf("%p", n)`, `*n = ...`, `n[0]`. A null test of `n` (`if (!n)`)
  and passing `n` to a `[[borrow]]` parameter do not;
- `&n->in` and assignments of whole struct members (`n->in = make()`);
- a field first used inside an `if` without braces, a `switch` case or a
  loop body after it (the merge at the end does not know it), or first
  used inside a nested block (it is forgotten at the end of the block).

Limitations of struct tracking:

- Owner fields of struct array elements (`struct buf a[4]`, also as a
  struct member), of array elements (`v[0]->data`), of structs reached
  through plain (unannotated) pointers (`struct node *q = ...; q->data`,
  `s.ptr->data` for a member without `[[owner]]`, `n->prev->data`) and
  through borrows (`m->data` for a `[[borrow_mut]]` `m`) are not tracked.
  Globals and statics are not tracked.
- Fields more than four member names deep (counting the members after
  `->` as well, e.g. `n->next->next->next->next->data`) are not tracked.
- An `[[owner]]` function parameter of type `void *` is taken to release
  the object without its fields (like `free()`); a reallocation function
  declared that way (`[[owner]] void *p` in a `realloc` wrapper) gets
  false "leak at release" errors.
- A field reached through an owner is created on first use only while
  fewer than 48 variables are tracked in the function; after that it is
  not tracked.
- `&s` releases all fields without checking (see above); a pointer taken
  with `&s` (`struct buf *ps = &s;`) is not connected to `s`.
- Pointer parameters: copies of the pointer (`struct buf *q = b;`) are not
  connected to it, so `free(b->data); q->data = NULL;` is still reported
  as left moved; `(*b).data` is not tracked. `[[borrow]]` and
  `[[borrow_mut]]` pointer parameters only track the pointer itself;
  `[[owner]]` pointer parameters also track the fields of the object (see
  *Through owner pointers*).
- Whole structs cannot be borrowed (`[[borrow]]` needs a pointer);
  `[[borrow]]` and `[[borrow_mut]]` struct members are not checked.
- An initializer of the form `(struct buf){0}` or any other expression than
  an initializer list or a struct variable makes all owner fields *owned*.
- Unions with owner members are tracked like structs (each member
  separately), which is usually wrong; do not use owners in unions.
- Each owner field counts towards the limit of 64 tracked variables per
  function.

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
- Liveness follows the source text, not the full control-flow graph, so it
  is sometimes stricter than Rust: a conflict in one branch of an `if` and a
  use in a later branch of the same `if` is an error, and a borrow that is
  reassigned in both branches of an `if` still counts as possibly holding
  the old value.
- `goto` is rejected by tcc-win unless `-fgoto` is given; with `-fgoto`,
  `goto` paths are not tracked (a backward `goto` that forms a loop does
  not keep a borrow alive).
- At most 128 conflicts are pending per function; beyond that a conflict is
  reported as an error right away (as if the borrow were used again).
- Taking the address (`&p`) marks `p` as owned, on all paths.
- Owners and borrows in globals and statics are not tracked; arrays only
  as described under *Arrays of owners* (constant indexes; any other use
  stops the checking of the array); in structs only `[[owner]]` fields of
  local struct variables, by-value parameters, objects behind pointer
  parameters and objects reached through owners are (see *Owners inside
  structs*).
- Only the null tests listed under *Null* narrow to *null*; for example
  `if (p == NULL || n == 0)` and `switch` do not.
- Return values of `[[owner]]` functions may be discarded silently.
- Infinite loops (`for (;;)`) are treated like loops that may run zero times.
- At most 64 owners and borrows per function (each tracked struct field
  and array element counts as one).
