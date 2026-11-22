#include <stdlib.h>
#include <stdio.h>
void f(char *s) { char *p = s; free(s); puts(p); }
