/* null tests that narrow an owner to "null" (nothing to release) */
#include <stdlib.h>
#include <stdio.h>

struct node { int v; struct node *next; };

static int eq_null(void)
{
    [[owner]] char *p = malloc(16);
    if (p == NULL)
        return 1;
    free(p);
    return 0;
}

static int null_eq(void)
{
    [[owner]] char *p = malloc(16);
    if (NULL == p)
        return 1;
    free(p);
    return 0;
}

static int eq_nullptr_zero(void)
{
    [[owner]] char *p = malloc(16);
    [[owner]] char *q;
    if (p == nullptr)
        return 1;
    q = malloc(16);
    if (0 == q) {
        free(p);
        return 1;
    }
    free(q);
    free(p);
    return 0;
}

static int parens_and_casts(void)
{
    [[owner]] char *p = malloc(16);
    if (((p) == ((void *)0)))
        return 1;
    free(p);
    return 0;
}

static int ne_null(void)
{
    [[owner]] char *p = malloc(16);
    if (p != NULL) {
        free(p);
        return 0;
    }
    return 1;               /* p is null here */
}

static int not_eq_null(void)
{
    [[owner]] char *p = malloc(16);
    if (!(p == NULL)) {
        free(p);
        return 0;
    }
    return 1;
}

/* NULL, (void *)0 and 0 initializers */
static int null_init(void)
{
    [[owner]] char *p = NULL;
    [[owner]] char *q = (void *)0, *r = 0;
    return 0;
}

static void list_free([[owner]] struct node *n)
{
    while (n != NULL) {         /* n is null when the loop ends */
        [[owner]] struct node *next = n->next;
        free(n);
        n = next;
    }
}

static void list_free_for([[owner]] struct node *n)
{
    for (; n; ) {
        [[owner]] struct node *next = n->next;
        free(n);
        n = next;
    }
}

static int retry(void)
{
    [[owner]] char *p = malloc(16);
    while (!p)                  /* p is null in the body: no leak */
        p = malloc(16);
    free(p);
    return 0;
}

int main(void)
{
    [[owner]] struct node *n = malloc(sizeof *n);
    if (n == NULL)
        return 1;
    n->v = 1;
    n->next = NULL;
    list_free(n);
    n = malloc(sizeof *n);
    if (!n)
        return 1;
    n->next = NULL;
    list_free_for(n);
    printf("%d\n", eq_null() + null_eq() + eq_nullptr_zero()
        + parens_and_casts() + ne_null() + not_eq_null() + null_init()
        + retry());
    return 0;
}
