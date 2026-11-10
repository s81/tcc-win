#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    {
        [[borrow_mut]] char *m = p;
        m[0] = 1;
        puts(p); /* error */
    }
    free(p);
    return 0;
}
