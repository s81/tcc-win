#include <stdlib.h>
#include <stdio.h>
char *g; void f(void) { g = malloc(4); g = malloc(5); }
