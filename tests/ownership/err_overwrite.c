#include <stdlib.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    p = malloc(8); /* error */
    free(p);
    return 0;
}
