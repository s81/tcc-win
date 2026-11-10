#include <stdlib.h>
int f(int x)
{
    [[owner]] char *p = malloc(4);
    if (x)
        return 1; /* error */
    free(p);
    return 0;
}
