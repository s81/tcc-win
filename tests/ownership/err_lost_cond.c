#include <stdlib.h>
/* an [[owner]] result used as a condition is lost */
int main(void)
{
    if (malloc(4))
        return 1;
    return 0;
}
