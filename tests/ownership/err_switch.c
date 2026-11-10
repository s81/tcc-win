#include <stdlib.h>
int f(int x)
{
    [[owner]] char *p = malloc(4);
    switch (x) {
    case 1:
        free(p);
        break;
    }
    return 0; /* error */
}
