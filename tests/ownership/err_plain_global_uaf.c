#include <stdlib.h>
#include <stdio.h>
char *g; void f(void) { free(g); g[0] = 1; }
