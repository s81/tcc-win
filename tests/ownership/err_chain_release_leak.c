#include <stdlib.h>
struct node { [[owner]] struct node *next; [[owner]] char *data; };
void f(void)
{
    [[owner]] struct node *n = malloc(sizeof *n);
    if (!n)
        return;
    n->next = NULL;
    n->data = malloc(16);
    free(n);
}
