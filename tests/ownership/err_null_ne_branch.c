/* 'p != NULL' is true: p is not null in the 'then' branch */
#include <stdlib.h>
int main(void)
{
    [[owner]] char *p = malloc(16);
    if (p != NULL)
        return 0;
    return 1;
}
