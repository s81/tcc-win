#include <stdlib.h>
/* a plain pointer that receives an owner result must be released */
int f(int n)
{
    char *p = malloc(n);
    if (!p)
        return -1;
    p[0] = 1;
    return 0;
}
