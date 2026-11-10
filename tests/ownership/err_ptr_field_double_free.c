#include <stdlib.h>
struct buf { [[owner]] char *data; };
void buf_free(struct buf *b) {
    free(b->data);
    free(b->data);
    b->data = nullptr;
}
