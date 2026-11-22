#include <stdlib.h>
[[owner]] char *g;
void f(void) { g = malloc(4); free(g); g[0] = 1; }
