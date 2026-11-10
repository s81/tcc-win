#include <stdlib.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    free(p);
    return p[0]; /* error */
}
