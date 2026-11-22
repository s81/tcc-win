#include <stdlib.h>
void f(void) { [[owner]] static char *s; s = malloc(2); s = malloc(3); }
