#include <stdlib.h>
struct buf { [[owner]] char *data; int len; };
void f(void)
{
    struct buf s = { malloc(4), 4 };
    free(s.data);
    s.data[0] = 1;
}
