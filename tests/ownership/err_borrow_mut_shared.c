#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    {
        [[borrow]] const char *s = p;
        [[borrow_mut]] char *m = p; /* error */
        m[0] = s[0];
    }
    free(p);
    return 0;
}
