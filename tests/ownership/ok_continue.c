/* valid uses of 'continue' and 'break' with owners */
#include <stdlib.h>
#include <stdio.h>

struct node { int v; struct node *next; };

/* released and replaced before 'continue' */
static int replace(void)
{
    [[owner]] char *p = malloc(16);
    for (int i = 0; i < 3; ++i) {
        if (i == 1) {
            free(p);
            p = malloc(16);
            continue;
        }
    }
    free(p);
    return 0;
}

/* owner declared in the body: null or released at every 'continue' */
static int body_owner(void)
{
    for (int i = 0; i < 3; ++i) {
        [[owner]] char *q = malloc(4);
        if (!q)
            continue;
        if (i == 1) {
            free(q);
            continue;
        }
        free(q);
    }
    return 0;
}

/* 'break' out of a switch inside a loop does not leave the loop body */
static int in_switch(int k)
{
    for (int i = 0; i < 2; ++i) {
        [[owner]] char *q = malloc(4);
        switch (k) {
        case 0:
            break;
        default:
            break;
        }
        free(q);
    }
    return 0;
}

/* 'continue' in 'while (n)': n is null when the loop ends */
static void list_free([[owner]] struct node *n)
{
    while (n) {
        [[owner]] struct node *next = n->next;
        free(n);
        n = next;
        if (n == NULL)
            continue;
    }
}

int main(void)
{
    [[owner]] struct node *n = malloc(sizeof *n);
    if (!n)
        return 1;
    n->next = NULL;
    list_free(n);
    printf("%d\n", replace() + body_owner() + in_switch(0) + in_switch(1));
    return 0;
}
