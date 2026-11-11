#include <stdlib.h>
void f(void)
{
    [[owner]] char *v[2] = { malloc(1), malloc(2) };
    free(v[0]);
    free(v[1]);
    free(v[0]);
}
