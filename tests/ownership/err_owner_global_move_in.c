#include <stdlib.h>
[[owner]] static char *g;
/* x is moved into g: using x afterwards is a use after move */
void f(void) { [[owner]] char *x = malloc(4); free(g); g = x; x[0] = 1; }
