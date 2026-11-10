#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[borrow]] const char *v;
    {
        [[owner]] char *q = nullptr;
        v = q;
    } /* error: q ends while v still borrows from it */
    puts(v);
    return 0;
}
