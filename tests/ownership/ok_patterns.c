/* valid ownership patterns that must compile without diagnostics */
#include <stdlib.h>
#include <stdio.h>

struct node { int v; struct node *next; };

static void node_free([[owner]] struct node *n) { free(n); }

static int init(char **out) { *out = malloc(8); return *out != nullptr; }

/* early return on allocation failure, release on every other path */
static int early(int x)
{
    [[owner]] char *p = malloc(16);
    if (!p)
        return -1;
    if (x) {
        free(p);
        return 1;
    }
    free(p);
    return 0;
}

/* owner (re)initialized through its address */
static int out_param(void)
{
    [[owner]] char *p;
    init(&p);              /* '&p' makes p owned */
    if (!p)
        return 1;
    free(p);
    return 0;
}

/* release in one branch, move in the other */
static struct node *branches(int keep)
{
    [[owner]] struct node *n = malloc(sizeof *n);
    [[owner]] struct node *r = nullptr;
    if (!n)
        return nullptr;
    n->v = 1, n->next = nullptr;      /* borrows */
    if (keep)
        r = n;
    else
        node_free(n);
    return r;
}

/* release and re-acquire inside a loop; break with consistent state */
static int loop(int n)
{
    __attribute__((owner)) char *p = malloc(4);
    while (n--) {
        free(p);
        p = malloc(4);
        if (!p)
            break;
    }
    free(p);
    return 0;
}

int main(void)
{
    [[tcc::owner]] struct node *n = branches(1);
    int r = early(0) + early(1) - 1 + out_param() + loop(3);
    if (n)
        node_free(n);
    printf("ok_patterns: %d\n", r);
    return r;
}
