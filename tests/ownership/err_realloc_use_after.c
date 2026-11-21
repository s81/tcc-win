#include <stdlib.h>
/* realloc succeeded: p was taken */
int main(void)
{
    [[owner]] char *p = malloc(4);
    if (!p)
        return 1;
    [[owner]] char *q = realloc(p, 8);
    if (!q) {
        free(p);
        return 1;
    }
    free(p);
    free(q);
    return 0;
}
