#include <stdio.h>
#include <string.h>
/* printf and strlen do not keep the pointer, so it is still leaked */
void f(const char *s)
{
    char *p;
    p = strdup(s);
    printf("%s %d\n", p, (int)strlen(p));
}
