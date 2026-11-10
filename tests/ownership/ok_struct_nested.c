/* [[owner]] fields of nested struct members: valid patterns */
#include <stdlib.h>
#include <string.h>

struct buf {
    [[owner]] char *data;
    size_t len;
};

struct conn {
    struct buf in, out;
    [[owner]] char *name;
};

struct server {
    struct conn c;
    int port;
};

struct anon {
    struct {
        [[owner]] char *x;     /* member of an anonymous struct: 'a.x' */
    };
    int n;
};

static void buf_free(struct buf *b) { free(b->data); b->data = nullptr; }

/* each field at each level */
static void fields(void)
{
    struct conn c = { { malloc(4), 4 }, { .data = nullptr }, malloc(8) };
    free(c.in.data);
    free(c.name);
    c.out.data = malloc(2);
    if (!c.out.data)
        return;
    free(c.out.data);
}

/* three levels, null tests and moves */
static int deep(void)
{
    struct server s = {};
    s.c.in.data = malloc(16);
    if (s.c.in.data == NULL)
        return -1;
    [[owner]] char *p = s.c.in.data;   /* moves s.c.in.data */
    free(p);
    s.c.in.data = malloc(1);           /* ok: it was moved */
    free(s.c.in.data);
    return 0;
}

/* '&s.a' hands the fields under s.a to the callee */
static void addr_member(void)
{
    struct conn c = { { malloc(1), 1 }, { malloc(2), 2 }, nullptr };
    buf_free(&c.in);
    buf_free(&c.out);
}

/* a member struct moves as a whole */
static struct buf take_in(void)
{
    struct conn c = { { malloc(1), 1 }, {}, nullptr };
    return c.in;                       /* moves c.in.data */
}

static void copy_member(void)
{
    struct conn c = { { malloc(1), 1 }, {}, nullptr };
    struct buf b = c.in;               /* moves c.in.data into b.data */
    free(b.data);
    struct buf d = take_in();
    c.out = d;                         /* d.data moved into c.out.data */
    free(c.out.data);
}

/* initializer of a member from a struct value */
static void init_member(void)
{
    struct buf b = { malloc(4), 4 };
    struct conn c = { .in = b };       /* b.data moved into c.in.data */
    free(c.in.data);
    struct buf z = {};
    struct conn d = { .out = z };      /* z.data was null: d.out.data null */
    (void)d.in.len;
}

/* the whole outer struct: '&s', return, by-value pass */
static void conn_free(struct conn *c)
{
    buf_free(&c->in);
    buf_free(&c->out);
    free(c->name);
    c->name = nullptr;
}

static void consume(struct conn c)
{
    free(c.in.data);
    free(c.out.data);
    free(c.name);
}

static struct conn make(void)
{
    struct conn c = { { malloc(1), 1 }, { malloc(1), 1 }, malloc(1) };
    return c;
}

static void whole(void)
{
    struct conn a = make();
    conn_free(&a);
    struct conn b = make();
    consume(b);
    struct conn c = make();
    struct conn d = c;                 /* moves all fields of c into d */
    conn_free(&d);
}

/* anonymous struct member */
static void anon(void)
{
    struct anon a = { .x = malloc(1) };
    free(a.x);
}

/* loops over nested fields */
static void loop(int n)
{
    struct server s = {};
    for (int i = 0; i < n; ++i) {
        s.c.name = malloc(8);
        if (!s.c.name)
            continue;
        free(s.c.name);
    }
}

int main(void)
{
    fields(); deep(); addr_member(); copy_member(); init_member(); whole();
    anon(); loop(2);
    return 0;
}
