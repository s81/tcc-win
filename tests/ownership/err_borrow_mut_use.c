#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    [[borrow_mut]] char *m = p;
    m[0] = 1;
    puts(p); /* error: m is used again below */
    m[1] = 2;
    free(p);
    return 0;
}
