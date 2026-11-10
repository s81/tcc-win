#include <stdlib.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    free(p);
    free(p); /* error */
    return 0;
}
