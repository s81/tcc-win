#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    [[borrow_mut]] char *m = p;
    printf("%s %s\n", m, p); /* error: m and p used together */
    free(p);
    return 0;
}
