#include <stdlib.h>
[[owner]] static char *g;
void f(void) { [[owner]] char *x = g; free(x); g[0] = 1; }
