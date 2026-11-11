#include <stdlib.h>
#include <stdio.h>
struct node { [[owner]] struct node *next; [[owner]] char *data; };
void f([[owner]] struct node *n)
{
    [[borrow]] const char *v = n->data;
    free(n);
    puts(v);
}
