#include <stdlib.h>
#include <string.h>
/* an [[owner]] result used by an operator is lost */
int main(void)
{
    return strdup("x")[0];
}
