/* only a single null test narrows: '&&' with another condition does not */
#include <stdlib.h>
int main(int argc, char **argv)
{
    [[owner]] char *p = malloc(16);
    if (p == NULL && argc > 1)
        return 1;
    free(p);
    return 0;
}
