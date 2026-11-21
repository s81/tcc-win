#include <stdio.h>
_Thread_local struct { int a; long long b; } ts = { 11, 1LL << 40 };
_Thread_local double td = 2.5;
_Thread_local float tf[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
_Thread_local char tbuf[64] = "init";
int main(void) { puts("M0"); fflush(stdout); printf("%d %f %f %s\n", ts.a, td, (double)tf[2], tbuf); puts("tls run: ok"); return 0; }
