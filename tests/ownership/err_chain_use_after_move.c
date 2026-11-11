#include <stdlib.h>
struct node { [[owner]] struct node *next; [[owner]] char *data; };
char f([[owner]] struct node *n)
{
    free(n->data);
    char c = n->data[0];
    free(n);
    return c;
}
