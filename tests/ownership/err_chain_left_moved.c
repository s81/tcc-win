#include <stdlib.h>
struct node { [[owner]] struct node *next; [[owner]] char *data; };
[[owner]] struct node *f([[owner]] struct node *n)
{
    free(n->data);
    return n;
}
