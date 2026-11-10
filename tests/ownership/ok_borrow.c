/* valid borrow patterns that must compile without diagnostics */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static size_t length([[borrow]] const char *s) { return strlen(s); }

static void upcase([[borrow_mut]] char *s)
{
    for (; *s; ++s)
        if (*s >= 'a' && *s <= 'z')
            *s -= 'a' - 'A';
}

/* returning a borrow parameter is allowed: it does not borrow a local owner */
static const char *same([[borrow]] const char *s) { return s; }

static void fill([[borrow_mut]] char *dst, [[borrow]] const char *src)
{
    strcpy(dst, src);
}

int main(void)
{
    [[owner]] char *a = malloc(16);
    [[owner]] char *b = malloc(16);
    if (!a) {
        free(b);
        return 1;
    }
    if (!b) {
        free(a);
        return 1;
    }
    strcpy(a, "hello");
    fill(b, a);                         /* mutable + shared borrow of different owners */
    upcase(b);                          /* temporary mutable borrow for the call */
    {
        [[borrow]] const char *v1 = a;
        [[borrow]] const char *v2 = a;  /* any number of shared borrows */
        [[borrow]] const char *v3 = v1; /* copy of a shared borrow */
        printf("%zu %s %s %s %s\n", length(v1), v2, v3, same(a), a);
    }                                   /* v1, v2, v3 end here */
    {
        [[borrow_mut]] char *m = b;
        m[0] = 'J';
        upcase(m);                      /* reborrow of m for the call */
        [[borrow]] const char *cur = nullptr;
        cur = a;                        /* a borrow can be (re)assigned */
        puts(cur);
    }
    free(a);                            /* nothing borrows a any more */
    {
        [[borrow]] const char *late;    /* initialized later */
        late = b;
        puts(late);
    }
    free(b);
    return 0;
}
