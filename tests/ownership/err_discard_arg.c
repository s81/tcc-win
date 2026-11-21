#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* an [[owner]] result passed to a parameter that does not take ownership */
int main(void)
{
    size_t n = strlen(strdup("x"));
    printf("%s\n", strdup("y"));
    free(strdup("z"));          /* ok: free takes ownership */
    return (int)n;
}
