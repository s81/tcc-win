#include <stdlib.h>
#include <stdio.h>
struct N { struct { char *q; } in; char *v[2]; }; void f(struct N *n) { free(n->v[1]); puts(n->v[1]); }
