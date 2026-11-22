#include <stdlib.h>
#include <stdio.h>
struct N { char *name; }; void f(struct N *n) { n->name = malloc(2); n->name = malloc(3); }
