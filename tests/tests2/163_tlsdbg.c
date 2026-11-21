#include <stdio.h>
static int counter(void) { static _Thread_local int n = 100; return ++n; }
int main(void) { puts("M0"); fflush(stdout); printf("%d %d\n", counter(), counter()); puts("tls run: ok"); return 0; }
