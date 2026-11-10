#include <stdlib.h>
struct buf { [[owner]] char *data; int len; };
int f(int x)
{
    struct buf s = { malloc(4), 4 };
    if (x)
        return 1;
    free(s.data);
    return 0;
}
