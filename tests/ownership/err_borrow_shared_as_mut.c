#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void upcase([[borrow_mut]] char *s);
int main(void)
{
    [[owner]] char *p = calloc(4, 1);
    {
        [[borrow]] const char *s = p;
        upcase((char *)s); /* not a plain name: unchecked */
        upcase(s); /* error */
    }
    free(p);
    return 0;
}
