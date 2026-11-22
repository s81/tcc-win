#include <stdlib.h>
[[owner]] static char *g;
void f(void) { g = malloc(4); g = malloc(8); }
