#include <stdlib.h>
#include <stdio.h>
void upcase([[borrow_mut]] char *s);
int f(int c)
{
    [[owner]] char *p = calloc(4, 1);
    [[owner]] char *q = calloc(4, 1);
    [[borrow]] const char *v = p;
    puts(v);
    upcase(p); /* error: v still borrows p below if c is 0 */
    if (c)
        v = q;
    puts(v);
    free(p);
    free(q);
    return 0;
}
