#include <stdlib.h>
#include <string.h>
[[owner]] char *make(void);
void *keep;
int main(void)
{
    malloc(4);
    strdup("x");
    make();
    realloc(NULL, 4);
    (void)calloc(1, 1);
    keep = malloc(4);
    free(malloc(4));
    free(keep);
    return 0;
}
