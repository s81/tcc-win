#include <stdlib.h>
int f(int x)
{
    [[owner]] char *p = malloc(4);
    if (x)
        free(p);
    return 0; /* error: possibly still owned */
}
