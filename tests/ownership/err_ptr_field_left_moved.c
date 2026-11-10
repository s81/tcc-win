#include <stdlib.h>
struct buf { [[owner]] char *data; };
struct conn { struct buf in; };
void conn_close(struct conn *c) {
    free(c->in.data);
}
