#include <stdio.h>

_Thread_local int ti = 7;

int main(void)
{
    puts("M0");
    fflush(stdout);
    printf("%d\n", ti);
    fflush(stdout);
    puts("tls run: ok");
    return 0;
}
