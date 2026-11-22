#include <stdlib.h>
#include <stdio.h>
struct N { char *name; }; void f(struct N *n) { free(n->name); n->name[0] = 0; }
