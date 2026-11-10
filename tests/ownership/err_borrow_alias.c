#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void fill([[borrow_mut]] char *dst, [[borrow]] const char *src);
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    fill(p, p); /* error: aliasing */
    free(p);
    return 0;
}
