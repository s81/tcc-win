#include <stdlib.h>
[[owner]] static char *g;
char *copy;
/* a copy of an [[owner]] is a borrow: g still owns the first block */
void f(void) { g = malloc(4); copy = g; g = malloc(8); }
