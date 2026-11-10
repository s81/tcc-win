/* leaving 'while (p)' by 'break' does not make p null */
#include <stdlib.h>
int main(int argc, char **argv)
{
    [[owner]] char *p = malloc(16);
    while (p) {
        if (argc > 1)
            break;
        free(p);
        p = nullptr;
    }
    return 0;
}
