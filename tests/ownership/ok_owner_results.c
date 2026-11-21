#include <stdlib.h>
#include <string.h>
/* [[owner]] results that are kept, handed on or discarded on purpose */
[[owner]] char *dup1(const char *s) { return strdup(s); }
[[owner]] char *dup2(const char *s) { return (char *)strdup(s); }
static char *pick(int c, const char *s) { return c ? strdup(s) : NULL; }
int main(void)
{
    char *p, *v[2] = { malloc(1), (char *)malloc(2) };
    (void)malloc(0);  /* explicit discard (leaks, on purpose) */
    free(v[0]);
    free(v[1]);
    free(strdup("x"));
    free(dup1("a"));
    free(dup2("b"));
    free(pick(1, "c"));
    if ((p = malloc(4)) == NULL)
        return 1;
    free(p);
    return (int)sizeof(*strdup("not evaluated")) - 1;
}
