#include <stdio.h>
_Thread_local int tz;
int main(void) { puts("M0"); fflush(stdout); tz = 3; printf("%d\n", tz); puts("tls run: ok"); return 0; }
