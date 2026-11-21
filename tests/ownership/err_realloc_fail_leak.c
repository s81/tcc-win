#include <stdlib.h>
/* realloc failed: p still owns its block */
int main(void)
{
    [[owner]] char *p = malloc(4);
    if (!p)
        return 1;
    [[owner]] char *q = realloc(p, 8);
    if (!q)
        return 1;
    free(q);
    return 0;
}
