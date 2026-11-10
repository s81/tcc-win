#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    {
        [[borrow]] char *s = p; /* error: not const */
        s[0] = 0;
    }
    free(p);
    return 0;
}
