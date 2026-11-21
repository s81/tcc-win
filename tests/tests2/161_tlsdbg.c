#include <stdio.h>
static int global = 42;
_Thread_local int *tp = &global;
int main(void) { puts("M0"); fflush(stdout); printf("%d\n", *tp); puts("tls run: ok"); return 0; }
