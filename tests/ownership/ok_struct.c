/* [[owner]] fields of local structs: valid patterns */
#include <stdlib.h>
#include <string.h>

struct buf {
    [[owner]] char *data;
    size_t len;
};

struct pair {
    [[owner]] char *a;
    [[owner]] char *b;
};

/* cleanup helper: '&s' hands the fields to the callee; the fields of
   the caller's object must not be left dangling (see ok_struct_ptr.c) */
static void pair_free(struct pair *p)
{
    free(p->a);
    free(p->b);
    p->a = nullptr;
    p->b = nullptr;
}

static void buf_init(struct buf *b) { b->data = malloc(16); b->len = 16; }

/* init + free of each field */
static void fields(void)
{
    struct pair p = { malloc(4), malloc(8) };
    free(p.a);
    free(p.b);
}

/* null-initialized fields need no release */
static void null_init(void)
{
    struct pair p = { .a = nullptr };   /* b is zero-initialized too */
    struct pair q = {};
    struct buf b = {0};
    (void)p.a, (void)q.b, (void)b.len;
}

/* early return on allocation failure */
static int early(void)
{
    struct buf s = { malloc(16), 16 };
    if (!s.data)
        return -1;
    memset(s.data, 0, s.len);
    free(s.data);
    return 0;
}

/* assignment of a field after an uninitialized declaration */
static int assign_field(void)
{
    struct buf s;
    s.data = malloc(4);
    s.len = 4;
    if (s.data == NULL)
        return 1;
    [[owner]] char *p = s.data;   /* moves s.data */
    free(p);
    s.data = malloc(8);           /* s.data was moved: no overwrite */
    free(s.data);
    return 0;
}

/* handing the struct to init/cleanup helpers by address */
static void helpers(void)
{
    struct pair p = { malloc(1), malloc(2) };
    pair_free(&p);
    struct buf b;
    buf_init(&b);
    if (b.data)
        b.data[0] = 0;
    free(b.data);
}

/* returning the struct moves its fields to the caller */
static struct pair make(void)
{
    struct pair p = { malloc(1), nullptr };
    p.b = malloc(2);
    return p;
}

/* a by-value struct parameter owns its fields */
static void consume(struct pair p)
{
    free(p.a);
    free(p.b);
}

static void pass(void)
{
    struct pair p = make();
    consume(p);                 /* moves p.a and p.b */
    struct pair q = make();
    struct pair r = q;          /* moves q's fields into r */
    pair_free(&r);
}

static char *take(void)
{
    struct buf s = { malloc(3), 3 };
    return s.data;              /* moves s.data */
}

/* loops: acquire and release in each iteration */
static void loop(int n)
{
    struct buf s = { nullptr, 0 };
    for (int i = 0; i < n; ++i) {
        s.data = malloc(8);
        if (!s.data)
            continue;
        free(s.data);
    }
    while (n-- > 0) {
        struct buf t = { malloc(1), 1 };
        if (!t.data)
            break;
        free(t.data);
    }
}

/* a field moved into a plain owner, then the struct reassigned */
static void reassign(void)
{
    struct pair p = make();
    [[owner]] char *x = p.a;
    free(x);
    free(p.b);
    p = make();
    pair_free(&p);
}

/* an owner moved into a field */
static void into_field(void)
{
    [[owner]] char *x = malloc(4);
    struct buf s = { .data = x, .len = 4 };   /* moves x */
    [[owner]] char *y = malloc(4);
    struct buf t = {};
    t.data = y;                                /* moves y */
    free(s.data);
    free(t.data);
}

int main(void)
{
    fields(); null_init(); early(); assign_field(); helpers(); pass();
    free(take()); loop(3); reassign(); into_field();
    return 0;
}
