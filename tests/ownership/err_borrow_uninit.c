#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[borrow]] const char *v;
    puts(v); /* error */
    return 0;
}
