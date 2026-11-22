#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void f(void) { char *p = malloc(8); char *q = strcpy(p, "x"); free(q); p[0] = 0; }
