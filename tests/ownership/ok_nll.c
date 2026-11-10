/* non-lexical lifetimes: borrows end at their last use */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void upcase([[borrow_mut]] char *s)
{
    for (; *s; ++s)
        if (*s >= 'a' && *s <= 'z')
            *s -= 'a' - 'A';
}

int main(void)
{
    [[owner]] char *p = malloc(16);
    if (!p)
        return 1;
    strcpy(p, "hello");

    /* a shared borrow ends at its last use ... */
    [[borrow]] const char *v = p;
    puts(v);
    upcase(p);                  /* ok: v is not used any more */

    /* ... and so does a mutable one */
    [[borrow_mut]] char *m = p;
    m[0] = 'J';
    puts(p);                    /* ok: m is not used any more */

    /* assigning a new value ends the old borrow */
    [[borrow]] const char *cur = p;
    puts(cur);
    [[owner]] char *q = malloc(4);
    if (!q) {
        free(p);                /* ok: cur is only assigned again later */
        return 1;
    }
    strcpy(q, "abc");
    cur = q;
    puts(cur);

    /* a copy of a borrow keeps borrowing from the owner, not from the copy */
    [[borrow]] const char *w;
    {
        [[borrow]] const char *inner = p;
        w = inner;
    }
    puts(w);

    /* a borrow used only before a loop does not block the loop */
    [[borrow_mut]] char *m2 = q;
    m2[0] = 'x';
    for (int i = 0; i < 2; i++)
        upcase(q);

    /* a borrow created and last used inside one iteration */
    for (int i = 0; i < 2; i++) {
        [[borrow]] const char *it = q;
        puts(it);
        upcase(q);
    }

    /* last use in one branch, conflict in the other */
    if (p[0] == 'J')
        puts(w);
    else
        upcase(p);

    free(q);
    free(p);                    /* ok: no borrow of p is used later */
    return 0;
}
