#include <stdio.h>
_Thread_local int ti = 7;
int main(void) { int *q; puts("M0"); fflush(stdout); q = &ti; *q = 5; printf("%d\n", ti); puts("tls run: ok"); return 0; }
