#include <stdlib.h>
#include <stdio.h>
void f(void) { char *p = malloc(4); char *q = p; q[0] = 0; }
