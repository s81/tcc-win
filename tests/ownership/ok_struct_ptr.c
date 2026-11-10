/* owner fields through pointer parameters ('ps->f'): valid patterns */
#include <stdlib.h>
#include <string.h>

struct buf {
    [[owner]] char *data;
    size_t len;
};

struct conn {
    struct buf in;
    [[owner]] char *name;
};

/* init function: the caller's fields may be uninitialized, so the
   first assignment of an untouched field is not an overwrite */
static int buf_init(struct buf *b, size_t n)
{
    b->data = malloc(n);
    b->len = n;
    return b->data ? 0 : -1;
}

/* destructor: release and null out */
static void buf_free(struct buf *b)
{
    if (!b)
        return;
    free(b->data);
    b->data = nullptr;
    b->len = 0;
}

/* release and move a new resource in */
static int buf_resize(struct buf *b, size_t n)
{
    [[owner]] char *p = malloc(n);
    if (!p)
        return -1;
    if (b->data)
        memcpy(p, b->data, b->len < n ? b->len : n);
    free(b->data);
    b->data = p;              /* moves p; b->data was released */
    b->len = n;
    return 0;
}

/* take the resource out and leave null behind */
static char *buf_take(struct buf *b)
{
    [[owner]] char *p = b->data;
    b->data = nullptr;
    return p;
}

/* nested member through the pointer */
static void conn_close(struct conn *c)
{
    free(c->in.data);
    c->in.data = nullptr;
    if (c->name) {
        free(c->name);
        c->name = nullptr;
    }
}

/* the fields are handed to the callee with '&ps->member' */
static void conn_close2(struct conn *c)
{
    buf_free(&c->in);
    free(c->name);
    c->name = nullptr;
}

/* passing the pointer itself hands all fields over */
static void conn_destroy(struct conn *c)
{
    free(c->name);
    free(c->in.data);
    free(c);                  /* the object is gone: nothing left dangling */
}

/* the field may be read and borrowed freely while owned */
static size_t conn_len(struct conn *c)
{
    return c->name ? strlen(c->name) : 0;
}

int main(void)
{
    struct buf b;
    if (buf_init(&b, 8) == 0 && buf_resize(&b, 16) == 0)
        free(buf_take(&b));
    buf_free(&b);
    struct conn *c = malloc(sizeof *c);
    if (!c)
        return 1;
    c->in.data = nullptr, c->in.len = 0, c->name = nullptr;
    (void)conn_len(c);
    conn_close(c);
    conn_close2(c);
    conn_destroy(c);
    return 0;
}
