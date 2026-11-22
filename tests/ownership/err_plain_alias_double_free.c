#include <stdlib.h>
#include <stdio.h>
void f(void) { char *p = malloc(4), *q; q = p; free(p); free(q); }
