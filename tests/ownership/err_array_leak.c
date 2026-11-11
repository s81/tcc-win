#include <stdlib.h>
int f(void)
{
    [[owner]] char *v[3] = { malloc(1), malloc(2) };
    free(v[0]);
    return 0;
}
