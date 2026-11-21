#include <stdlib.h>
#include <string.h>
/* a cast to a pointer type keeps an [[owner]] result an owner */
int main(void)
{
    return (int)strlen((char *)strdup("x"));
}
