/* plain pointers (not [[owner]]): owner results kept in nested blocks,
   in members of local structs, in elements of local arrays and in
   globals; uses after release that are not errors */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct S { int n; char *p; struct { char *q; } in; };
static char *saved[4];
static int nsaved;
static void keep(void *p) { saved[nsaved++] = p; }       /* may keep it */
static void keep_s(struct S *s) { keep(s->p); }
static char *g;
static char *sg;

/* nested blocks */
static void n_free_after(int c) { char *p = NULL; if (c) p = malloc(3); free(p); }
static void n_stackbuf(int big)
{
    char b[8], *p = b;
    if (big)
        p = malloc(64);
    p[0] = 1;
    if (p != b)
        free(p);
}
static void n_kept(int c) { char *p; if (c) { p = malloc(3); keep(p); } }
static void n_loop(int n)
{
    char *p = 0;
    int i;
    for (i = 0; i < n; i++)
        if (!p)
            p = malloc(4);
    free(p);
}
static void n_break(int c) { char *p = 0; while (c--) { p = strdup("x"); break; } free(p); }
static void n_switch(int c)
{
    char *p = 0;
    switch (c) {
    case 1: p = malloc(1); break;
    case 2: { p = malloc(2); } break;
    }
    free(p);
}
static int n_cond_assign(void)
{
    char *p;
    if ((p = malloc(4)) == NULL)
        return -1;
    free(p);
    if (!(p = strdup("y")))
        return -1;
    free(p);
    return 0;
}

/* use after release */
static void u_maybe(char *p, int c) { if (c) free(p); else p[0] = 1; }
static void u_reassign(char *p) { free(p); p = malloc(2); free(p); }
static void u_null(char *p) { free(p); p = NULL; (void)p; }
static void u_list(char **head)
{
    char *p = *head;
    while (p) {
        char *n = *(char **)p;
        free(p);
        p = n;
    }
    *head = NULL;
}
static void u_early(int c) { char *p = malloc(2); if (c) { free(p); return; } p[0] = 0; free(p); }
static int u_value(void)
{
    char *p = malloc(8), *old = p, *q;
    q = realloc(p, 64);         /* the old value may be compared ... */
    if (!q)
        return free(p), -1;
    int moved = q != old;       /* ... (reading 'old' is not an error) */
    free(q);
    return moved >= 0 ? 0 : 1;
}

/* members and elements */
static void s_freed(void) { struct S s; s.p = malloc(4); free(s.p); }
static void s_kept(void) { struct S s; s.p = malloc(4); keep_s(&s); }
static struct S s_returned(void) { struct S s = {0}; s.p = malloc(4); return s; }
static int s_null(void) { struct S s; s.p = malloc(4); if (!s.p) return -1; free(s.p); return 0; }
static void s_nested(void) { struct S s; s.in.q = strdup("x"); puts(s.in.q); free(s.in.q); }
static void a_loop(void)
{
    char *a[4];
    int i;
    a[0] = malloc(4), a[1] = malloc(4), a[2] = a[3] = NULL;
    for (i = 0; i < 4; i++)
        free(a[i]);
}
static void a_kept(void) { char *a[2]; a[1] = malloc(4); keep(a[1]); }

/* globals: a global keeps its resource beyond the function */
static void g_set(void) { free(g); g = malloc(4); }
static void g_call(void) { g = malloc(4); keep(g); g = malloc(5); free(g); g = NULL; }
static void g_static(void) { sg = malloc(1); free(sg); sg = NULL; }

int main(void)
{
    struct S s;
    char *list = NULL;
    int i;
    n_free_after(1); n_stackbuf(1); n_stackbuf(0); n_kept(1); n_loop(3);
    n_break(2); n_switch(1); n_switch(2); n_switch(3);
    if (n_cond_assign())
        return 1;
    { char *m = malloc(2); u_maybe(m, 1); }
    { char *m = malloc(2); u_maybe(m, 0); free(m); }
    { char *m = malloc(1); u_reassign(m); }
    { char *m = malloc(1); u_null(m); }
    u_list(&list); u_early(1); u_early(0);
    if (u_value())
        return 1;
    s_freed(); s_kept();
    s = s_returned();
    free(s.p);
    if (s_null())
        return 1;
    s_nested(); a_loop(); a_kept();
    g_set(); g_set(); g_call(); g_static();
    free(g);
    for (i = 0; i < nsaved; i++)
        free(saved[i]);
    return 0;
}
