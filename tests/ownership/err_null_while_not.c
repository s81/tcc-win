/* p is not null after 'while (!p)' */
#include <stdlib.h>
int main(void)
{
    [[owner]] char *p = nullptr;
    while (!p)
        p = malloc(16);
    return 0;
}
