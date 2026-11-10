#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    [[borrow]] const char *v;
    {
        [[borrow]] const char *w = p;
        v = w;
    } /* error: w ends while v borrows from it */
    puts(v);
    free(p);
    return 0;
}
