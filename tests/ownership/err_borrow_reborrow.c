#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    {
        [[borrow_mut]] char *m = p;
        [[borrow_mut]] char *m2 = m;
        m2[0] = 1;
        m[1] = 2; /* error: m is mutably borrowed by m2 */
    }
    free(p);
    return 0;
}
