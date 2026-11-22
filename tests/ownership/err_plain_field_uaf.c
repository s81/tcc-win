#include <stdlib.h>
#include <stdio.h>
struct S { char *p; }; void f(void) { struct S s; s.p = malloc(4); free(s.p); s.p[0] = 1; }
