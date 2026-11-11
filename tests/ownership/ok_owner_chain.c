/* owner fields reached through owner pointers ('n->data', 'n->next->data',
   'l.head->data'): valid patterns */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct node {
    [[owner]] struct node *next;
    [[owner]] char *data;
};

struct buf {
    [[owner]] char *data;
    size_t len;
};

struct conn {
    struct buf in;
    [[owner]] struct conn *peer;
};

static size_t len([[borrow]] const char *s) { return strlen(s); }
static void show([[borrow]] const struct node *n) { puts(n->data ? n->data : "-"); }
static void clear([[borrow_mut]] struct node *n) { n->data = NULL; }
static void init_node(struct node *n) { n->next = NULL; n->data = NULL; }

static void list_free([[owner]] struct node *n)
{
    while (n) {
        [[owner]] struct node *next = n->next;   /* moves n->next */
        free(n->data);
        free(n);                                 /* both fields moved: ok */
        n = next;
    }
}

static void node_free([[owner]] struct node *n)
{
    if (!n)
        return;
    free(n->data);
    n->data = NULL;
    list_free(n->next);
    n->next = NULL;
    free(n);
}

static struct node *node_new(const char *s)
{
    [[owner]] struct node *n = malloc(sizeof *n);
    if (!n)
        return NULL;
    n->next = NULL;
    n->data = malloc(strlen(s) + 1);   /* first assignment: no overwrite */
    if (!n->data) {
        free(n);                       /* n->data is null here */
        return NULL;
    }
    strcpy(n->data, s);
    return n;                          /* the fields go with n */
}

static struct node *push([[owner]] struct node *head, const char *s)
{
    [[owner]] struct node *n = node_new(s);
    if (!n)
        return head;
    n->next = head;                    /* moves head into n->next */
    return n;
}

static void replace([[owner]] struct node *n, [[owner]] char *d)
{
    free(n->data);
    n->data = d;                       /* n->data was moved: ok */
    node_free(n);
}

/* a field moved on one path only, and set again on all paths */
static void branches([[owner]] struct node *n, int c)
{
    if (c)
        free(n->data);
    else {
        free(n->data);
    }
    n->data = NULL;
    show(n);
    node_free(n);
}

static void in_loop([[owner]] struct node *n, int k)
{
    for (int i = 0; i < k; ++i) {
        free(n->data);
        n->data = malloc(10);
    }
    node_free(n);
}

/* code the checker cannot follow makes the fields unknown */
static void aliases([[owner]] struct node *n)
{
    free(n->data);
    struct node *alias = n;            /* plain copy of the pointer */
    alias->data = NULL;
    printf("%p\n", (void *)n->data);
    free(n->data);
    init_node(n);                      /* unannotated parameter */
    printf("%p\n", (void *)n->data);
    free(n->data);
    clear(n);                          /* [[borrow_mut]] parameter */
    free(n->data);
    *n = (struct node){0};             /* write through the pointer */
    n->data = malloc(1);
    free(n->data);
    n->data = NULL;
    node_free(n);
}

static void borrows([[owner]] struct node *n)
{
    [[borrow]] const char *v = n->data;
    if (v)
        printf("%s %zu\n", v, len(n->data));
    node_free(n);                      /* v is not used any more */
}

/* chains: 'c->peer->in.data' */
static void conn_free([[owner]] struct conn *c)
{
    free(c->in.data);
    c->in.data = NULL;
    if (c->peer) {
        free(c->peer->in.data);
        c->peer->in.data = NULL;
        free(c->peer->peer);
        c->peer->peer = NULL;
    }
    free(c->peer);
    free(c);
}

/* through an owner member of a local struct */
static void list_head(void)
{
    struct { [[owner]] struct node *head; } l = { 0 };
    l.head = node_new("a");
    if (!l.head)
        return;
    free(l.head->data);
    l.head->data = NULL;
    node_free(l.head);
}

/* through a pointer parameter: 'b->peer' is the caller's */
static void conn_reset(struct conn *c)
{
    if (!c->peer)
        return;
    free(c->peer->in.data);
    c->peer->in.data = NULL;
}

static void reassign(void)
{
    [[owner]] struct node *n = node_new("x");
    if (!n)
        return;
    free(n->data);
    n->data = NULL;
    node_free(n);
    n = node_new("y");                 /* new object: its fields are new */
    if (n) {
        free(n->data);
        n->data = NULL;
    }
    node_free(n);
}

int main(void)
{
    [[owner]] struct node *l = push(push(NULL, "a"), "b");
    if (l) {
        [[owner]] char *d = malloc(4);
        if (d) {
            strcpy(d, "new");
            [[owner]] struct node *first = l;
            l = first->next;
            first->next = NULL;
            replace(first, d);
        } else
            free(d);
    }
    list_free(l);
    branches(node_new("c"), 1);
    in_loop(node_new("d"), 2);
    aliases(node_new("e"));
    borrows(node_new("f"));
    list_head();
    reassign();
    [[owner]] struct conn *c = calloc(1, sizeof *c);
    if (c)
        conn_reset(c);
    conn_free(c);
    return 0;
}
