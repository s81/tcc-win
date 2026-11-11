/* local arrays of owners: valid patterns */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static size_t len([[borrow]] const char *s) { return strlen(s); }

static int constant_indexes(void)
{
    [[owner]] char *v[3] = { malloc(1), malloc(2) };   /* v[2] is null */
    if (!v[0] || !v[1]) {
        free(v[0]);
        free(v[1]);
        return -1;
    }
    free(v[0]);
    v[0] = malloc(3);           /* v[0] was moved: no overwrite */
    if (!v[2])
        v[2] = malloc(4);       /* null in the 'then' branch */
    [[owner]] char *p = v[2];   /* moves v[2] */
    v[2] = nullptr;
    free(p);
    free(v[1]);
    free(v[0]);
    return 0;
}

static void moves_in_and_out(void)
{
    [[owner]] char *a = malloc(1);
    [[owner]] char *v[2] = { a, NULL };   /* moves a into v[0] */
    [[owner]] char *b = malloc(1);
    v[1] = b;                             /* moves b into v[1] */
    free(v[0]);
    free(v[1]);
    [[owner]] char *w[] = { malloc(1) };  /* size from the initializer */
    [[owner]] char *c = w[0];
    w[0] = NULL;
    free(c);
}

/* a non-constant index (or any other use of the array) stops the
   checking of its elements: loops are fine */
static void loops(int n)
{
    [[owner]] char *u[4];
    for (int i = 0; i < 4; ++i)
        u[i] = malloc(8);
    for (int i = 0; i < 4; ++i)
        free(u[i]);
    [[owner]] char *x[2] = { malloc(1), malloc(1) };
    for (int i = 0; i < n && i < 2; ++i)
        free(x[i]);
    [[owner]] char *z[2] = { malloc(1), malloc(1) };
    memset(z, 0, sizeof z);               /* 'z' itself: untracked */
}

static void borrow_and_address(void)
{
    [[owner]] char *v[2] = {};
    char **pp = &v[1];                    /* '&v[1]': may be set through pp */
    *pp = malloc(4);
    if (v[1]) {
        strcpy(v[1], "abc");
        printf("%zu\n", len(v[1]));
        [[borrow]] const char *view = v[1];
        puts(view);
    }
    free(v[1]);
    printf("%d\n", (int)sizeof v[0]);     /* sizeof is not a use */
}

static void big_arrays(void)
{
    [[owner]] char *big[40];              /* more than 16 elements: untracked */
    big[3] = malloc(1);
    free(big[3]);
}

int main(void)
{
    constant_indexes();
    moves_in_and_out();
    loops(2);
    borrow_and_address();
    big_arrays();
    return 0;
}
