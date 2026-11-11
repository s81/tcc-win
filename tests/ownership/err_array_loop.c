#include <stdlib.h>
void f(int n)
{
    [[owner]] char *v[2] = { malloc(1) };
    for (int i = 0; i < n; ++i)
        free(v[0]);
}
