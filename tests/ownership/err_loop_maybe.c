/* released on some iterations only: the next iteration may use it */
#include <stdlib.h>
int main(void)
{
    [[owner]] char *p = malloc(16);
    for (int i = 0; i < 3; ++i)
        if (i == 1)
            free(p);
    return 0;
}
