#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    {
        [[borrow_mut]] char *m1 = p;
        [[borrow_mut]] char *m2 = p; /* error */
        m1[0] = m2[0] = 0;
    }
    free(p);
    return 0;
}
