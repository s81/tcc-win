#include <stdlib.h>
struct buf { [[owner]] char *data; };
struct conn { struct buf in, out; };
int main(void) {
    struct conn c = { .in = { malloc(4) } };
    free(c.out.data);
    return 0;
}
