#include <stdio.h>
#include <pthread.h>
_Thread_local int ti = 7;
static void *w(void *a) { ti += 1; printf("w %d\n", ti); fflush(stdout); return a; }
int main(void) { pthread_t h; puts("M0"); fflush(stdout); pthread_create(&h, 0, w, 0); pthread_join(h, 0); printf("%d\n", ti); puts("tls run: ok"); return 0; }
