#include <stdlib.h>
#include <string.h>
#include <stdio.h>
const char *f(void)
{
    [[owner]] char *p = malloc(4);
    [[borrow]] const char *v = p;
    return v; /* error: dangling */
}
