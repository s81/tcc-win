#include <stdlib.h>
#include <stdio.h>
void f(char *s) { char *q; q = &s[3]; free(s); *q = 0; }
