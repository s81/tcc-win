#include <stdlib.h>
void f(void)
{
    char *p = malloc(4);
    p = malloc(8);
    free(p);
}
