#include <stdlib.h>
int main(void)
{
    [[owner]] char *a = malloc(4);
    [[owner]] char *b = a;
    a[0] = 1; /* error */
    free(b);
    return 0;
}
