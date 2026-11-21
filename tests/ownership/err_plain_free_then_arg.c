#include <stdlib.h>
#include <stdio.h>
void f(char *p) { free(p); printf("%s", p); }
