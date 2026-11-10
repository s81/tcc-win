#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void upcase([[borrow_mut]] char *s);
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    [[borrow]] const char *v = p;
    while (v[0] != 0x7f) {
        upcase(p); /* error: the loop condition uses v again */
    }
    free(p);
    return 0;
}
