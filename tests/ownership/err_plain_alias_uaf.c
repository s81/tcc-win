#include <stdlib.h>
#include <stdio.h>
void f(void) { char *p = malloc(4); char *q = p; free(q); p[0] = 1; }
