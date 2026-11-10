/* several independent ownership errors are all reported in one run,
   at most one per variable (the third free(p) is not reported again) */
#include <stdlib.h>
#include <string.h>

void upcase([[borrow_mut]] char *s);

static void leak(void)
{
    [[owner]] char *p = malloc(16);
}                                       /* 1: leak at end of scope */

static void twice(void)
{
    [[owner]] char *p = malloc(16);
    free(p);
    free(p);                            /* 2: use after move */
    free(p);                            /* (same variable: not repeated) */
}

static void borrowed(void)
{
    [[owner]] char *p = malloc(16);
    [[borrow]] const char *v = p;
    free(p);                            /* 3: moved while borrowed by v */
    (void)strlen(v);
}

int main(void)
{
    [[owner]] char *q = malloc(16);
    upcase(q);
    return 0;                           /* 4: leak at return */
}
