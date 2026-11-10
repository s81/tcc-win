#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void f([[borrow_mut]] char *s)
{
    free(s); /* error: s is not owned */
}
