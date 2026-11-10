#include <stdlib.h>
struct buf { [[owner]] char *data; int len; };
void f(void)
{
    struct buf s = { malloc(4), 4 };
    struct buf t;
    t = s;
    free(t.data);
    free(s.data);
}
