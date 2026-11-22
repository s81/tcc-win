#include <stdlib.h>
#include <stdio.h>
void f(void) { char *p = malloc(4); free(p); p[0] = 1; }
