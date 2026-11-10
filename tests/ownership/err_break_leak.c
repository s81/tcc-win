/* an owner declared in the loop body goes out of scope at 'break' */
#include <stdlib.h>
int main(void)
{
    for (int i = 0; i < 3; ++i) {
        [[owner]] char *q = malloc(4);
        if (i == 1)
            break;            /* q leaks */
        free(q);
    }
    return 0;
}
