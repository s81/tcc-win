#include <stdlib.h>
#include <string.h>
#include <stdio.h>

[[owner]] char *dup(const char *s)
{
    [[owner]] char *p = malloc(strlen(s) + 1);
    if (!p)
        return nullptr;
    strcpy(p, s);
    return p;              /* ownership moves to the caller */
}

void consume([[owner]] char *s)
{
    puts(s);               /* borrow */
    free(s);               /* release */
}

int main(void)
{
    [[owner]] char *a = dup("hello");
    [[owner]] char *b = a; /* move: a is dead now */
    [[owner]] char *c;
    c = dup("world");
    if (b) {
        printf("%s %s\n", b, c);
        free(b);
    }
    consume(c);
    [[owner]] char *d = dup("again");
    free(d);
    d = dup("reuse");      /* re-initialization after release is fine */
    for (int i = 0; i < 3; i++) {
        [[owner]] char *t = dup("loop");
        free(t);
    }
    switch (d[0]) {
    case 'r':
        free(d);
        break;
    default:
        consume(d);
        break;
    }
    return 0;
}
