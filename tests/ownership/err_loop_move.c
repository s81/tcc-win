#include <stdlib.h>
int f(int n)
{
    [[owner]] char *p = malloc(4);
    while (n--)
        free(p); /* error */
    return 0;
}
