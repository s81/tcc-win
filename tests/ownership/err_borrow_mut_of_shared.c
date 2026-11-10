#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    {
        [[borrow]] const char *s = p;
        [[borrow_mut]] char *m = (char *)0;
        m = s; /* error */
    }
    free(p);
    return 0;
}
