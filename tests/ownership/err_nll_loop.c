#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void upcase([[borrow_mut]] char *s);
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    [[borrow]] const char *v = p;
    for (int i = 0; i < 3; i++) {
        puts(v);
        upcase(p); /* error: v is used again in the next iteration */
    }
    free(p);
    return 0;
}
