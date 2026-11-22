/* pointers derived from plain pointers ('q = p + n', 'strchr(p, c)'):
   valid patterns */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void use_before(void) { char *p = malloc(8); char *q = p + 2; q[0] = 1; free(p); }
static void copy_alias(void) { char *p = malloc(8); char *q = strcpy(p, "x"); puts(q); free(q); }
static void maybe(int c)
{
    char b[4], *p = malloc(8), *q = b;
    if (c)
        q = p + 1;
    free(p);
    q = b;
    q[0] = 0;
}
static void cut(char *s) { char *q = strchr(s, ':'); if (q) *q = 0; puts(s); free(s); }
static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && e[-1] == ' ')
        --e;
    *e = 0;
    free(s);
}
static void reset(void) { char *p = malloc(8); char *q = strstr(strcpy(p, "ab"), "b"); q = 0; free(p); (void)q; }

int main(void)
{
    use_before(); copy_alias(); maybe(0); maybe(1);
    { char *s = strdup("a:b"); cut(s); }
    { char *s = strdup("x  "); trim(s); }
    reset();
    return 0;
}
