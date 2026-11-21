/* C23: a variable length array may be initialized with {} */
#include <string.h>

static int sum(int n)
{
    char junk[256];
    int a[n] = {}, i, s = 0;
    memset(junk, 0x55, sizeof junk);
    for (i = 0; i < n; i++)
        s += a[i];
    return s;
}

static int sum2(int n)
{
    int b[n][3] = {}, i, j, s = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < 3; j++)
            s += b[i][j];
    return s;
}

int main(void)
{
    char dirty[512];
    memset(dirty, 0x77, sizeof dirty);
    return sum(40) + sum2(7) + (dirty[0] == 0x77 ? 0 : 1);
}
