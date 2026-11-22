/* aliases of plain pointers ('q = p') and members reached through a
   pointer ('n->name'): valid patterns */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct N { struct N *next; char *name; };
static void *kept;
static void keep(void *p) { free(kept); kept = p; }

static void free_by_alias(void) { char *p = malloc(4); char *q = p; q[0] = 1; free(q); }
static void inner_alias(void) { char *p = malloc(4); { char *q = p; q[0] = 0; } free(p); }
static void outer_alias(void) { char *q; { char *p = malloc(4); q = p; } free(q); }
static void alias_kept(void) { char *p = malloc(4); char *q = p; keep(q); p[0] = 1; }
static void maybe_alias(int c) { char *p = malloc(4), *q = 0; if (c) q = p; free(p); }
static void maybe_alias2(int c)
{
    char b[4], *p = malloc(4), *q = b;
    if (c)
        q = p;
    if (q != b)
        q = b;
    free(p);
    q[0] = 1;
}
static void alias_reset(void) { char *p = malloc(4); char *q = p; p = 0; free(q); }
static char *alias_returned(void) { char *p = malloc(4); char *q = p; return q; }
static void alias_null(void) { char *p = malloc(4), *q = p; if (!q) return; free(p); }
static void grow(void) { char *p = malloc(4), *q; q = realloc(p, 8); if (q) p = q; free(p); }

static void set_name(struct N *n, const char *s) { free(n->name); n->name = strdup(s); }
static void clear_name(struct N *n) { free(n->name); n->name = NULL; }
static int new_name(struct N *n) { n->name = malloc(3); if (!n->name) return -1; n->name[0] = 0; return 0; }
static void free_list(struct N *n)
{
    while (n) {
        struct N *x = n->next;
        free(n->name);
        free(n);
        n = x;
    }
}
static void next_object(struct N *n) { free(n->name); n->name = NULL; n = n->next; if (n) n->name[0] = 'x'; }
static void steal_name(struct N *n) { char *s = n->name; n->name = NULL; free(s); }

int main(void)
{
    struct N *a = calloc(1, sizeof *a), *b = calloc(1, sizeof *b);
    free_by_alias(); inner_alias(); outer_alias(); alias_kept();
    maybe_alias(0); maybe_alias(1); maybe_alias2(0); maybe_alias2(1);
    alias_reset(); free(alias_returned()); alias_null(); grow();
    if (!a || !b)
        return 1;
    a->next = b;
    set_name(a, "a"); set_name(b, "b"); set_name(a, "c");
    clear_name(a);
    if (new_name(a))
        return 1;
    next_object(a);
    steal_name(b);
    set_name(b, "d");
    free_list(a);
    free(kept);
    return 0;
}
