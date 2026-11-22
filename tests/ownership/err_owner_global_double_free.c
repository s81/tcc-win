#include <stdlib.h>
[[owner]] char *g;
void f(void) { free(g); free(g); }
