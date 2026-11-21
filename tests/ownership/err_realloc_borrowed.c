#include <stdlib.h>
/* realloc moves p (borrowed by v), then p gets the result */
int main(void)
{
    [[owner]] char *p = malloc(4);
    if (!p)
        return 1;
    [[borrow]] const char *v = p;
    p = realloc(p, 8);
    char c = v[0];
    free(p);
    return c;
}
