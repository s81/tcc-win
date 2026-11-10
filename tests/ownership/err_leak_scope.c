#include <stdlib.h>
int main(void)
{
    {
        [[owner]] char *p = malloc(4);
        (void)p;
    } /* error */
    return 0;
}
