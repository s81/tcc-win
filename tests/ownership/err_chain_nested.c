#include <stdlib.h>
struct node { [[owner]] struct node *next; [[owner]] char *data; };
struct list { [[owner]] struct node *head; };
void f(struct list *l)
{
    free(l->head->next->data);
    free(l->head->next->data);
}
