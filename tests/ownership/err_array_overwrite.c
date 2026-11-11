#include <stdlib.h>
void f(void)
{
    [[owner]] char *v[2] = {};
    v[1] = malloc(1);
    v[1] = malloc(2);
    free(v[1]);
}
