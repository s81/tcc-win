#include <stdlib.h>
#include <stdio.h>
void f(int c) { char *p; if (c) { p = malloc(3); p[0] = 0; } }
