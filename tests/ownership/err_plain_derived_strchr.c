#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void f(char *s) { char *q = strchr(s, ':'); free(s); puts(q); }
