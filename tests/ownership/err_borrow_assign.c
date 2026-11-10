#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = nullptr;
    [[borrow]] const char *v = p;
    p = malloc(4); /* error */
    free(p);
    return v != 0;
}
