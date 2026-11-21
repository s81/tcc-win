/* plain (non-[[owner]]) pointer locals that receive an [[owner]] result
   are tracked implicitly: freeing, returning or handing the pointer away
   in any other way is fine; only a pointer that is certainly never released
   nor stored anywhere is a leak (see err_implicit_*.c). */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct node { struct node *next; char *name; };
static char *global;
void keep(void *p);             /* unknown: may keep the pointer */

char *ret(const char *s) { char *p = strdup(s); return p; }
void freed(void) { char *p = malloc(8); if (!p) return; p[0] = 0; free(p); }
void stored(struct node *n, const char *s) { char *p = strdup(s); n->name = p; }
void global_store(void) { char *p = malloc(4); global = p; }
void escaped(void) { char *p = malloc(4); keep(p); }
void assigned_later(void) { char *p; p = malloc(4); free(p); }
void list(struct node **head)
{
    struct node *n = malloc(sizeof *n);
    if (!n) return;
    n->next = *head;
    *head = n;
}
void loop(void)
{
    int i;
    for (i = 0; i < 3; i++) {
        char *p = malloc(16);
        snprintf(p, 16, "%d", i);
        puts(p);
        free(p);
    }
}
void both(int c)
{
    char *p = malloc(4);
    if (c) { free(p); return; }
    free(p);
}
void grow(void)
{
    char *p = malloc(4), *q;
    q = realloc(p, 8);
    if (q) p = q;
    free(p);
}
char *alias(void) { char *p = malloc(4); char *q = p; return q; }

int main(void)
{
    struct node n, *h = NULL;
    char *s = ret("x");
    free(s);
    freed(); stored(&n, "a"); free(n.name); global_store(); free(global);
    assigned_later(); list(&h); free(h); loop(); both(0); grow();
    free(alias());
    return 0;
}
void keep(void *p) { free(p); }
