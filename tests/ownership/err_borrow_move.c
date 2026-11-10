#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    [[owner]] char *p = malloc(4);
    [[borrow]] const char *v = p;
    free(p); /* error: v still borrows p */
    return v != 0;
}
