#include <stdlib.h>
/* realloc takes its argument only if it returns non-null ([[tcc::realloc]]) */

/* p = realloc(p, n): p gets the result */
static void same(void)
{
    [[owner]] char *p = malloc(4);
    if (!p)
        return;
    p = realloc(p, 8);
    free(p);
}

/* with a temporary: on failure p is still owned and must be released */
static int tmp(void)
{
    [[owner]] char *p = malloc(4);
    if (!p)
        return 1;
    [[owner]] char *q = realloc(p, 8);
    if (!q) {
        free(p);
        return 1;
    }
    free(q);
    return 0;
}

/* assignment, cast, '== NULL' and the else branch */
static int assign(void)
{
    [[owner]] char *p = malloc(4);
    if (!p)
        return 1;
    [[owner]] char *q = NULL;
    q = (char *)realloc(p, 8);
    if (q != NULL)
        p = q;
    free(p);
    return 0;
}

/* growing in a loop */
static char *grow(size_t n)
{
    [[owner]] char *p = malloc(1);
    if (!p)
        return NULL;
    for (size_t cap = 2; cap < n; cap *= 2) {
        [[owner]] char *q = realloc(p, cap);
        if (!q) {
            free(p);
            return NULL;
        }
        p = q;
    }
    return p;
}

/* a field behind a pointer parameter keeps its value on failure */
struct buf { [[owner]] char *data; size_t len; };
static int buf_grow(struct buf *b, size_t n)
{
    [[owner]] char *q = realloc(b->data, n);
    if (!q)
        return -1;
    b->data = q;
    b->len = n;
    return 0;
}

int main(void)
{
    struct buf b = { NULL, 0 };
    same();
    if (tmp() || assign())
        return 1;
    free(grow(100));
    if (buf_grow(&b, 16))
        return 1;
    free(b.data);
    return 0;
}
