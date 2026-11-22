#include <stdlib.h>
#include <stdio.h>
void f(char *s) { char *q = s + 1, *r = q + 1; free(s); r[0] = 0; }
