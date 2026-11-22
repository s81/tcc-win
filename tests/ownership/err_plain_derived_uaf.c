#include <stdlib.h>
#include <stdio.h>
void f(void) { char *p = malloc(8); char *q = p + 2; free(p); q[0] = 1; }
