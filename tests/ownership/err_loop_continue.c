/* the state at 'continue' reaches the next iteration */
#include <stdlib.h>
int main(void)
{
    [[owner]] char *p = malloc(16);
    for (int i = 0; i < 3; ++i) {
        if (i == 1) {
            free(p);        /* freed, then the next iteration runs */
            continue;
        }
    }
    free(p);                /* double free on every run */
    return 0;
}
