/* [[owner]] globals and statics: correct uses */
#include <stdlib.h>
#include <string.h>

[[owner]] static char *g;
[[owner]] char *pub;
[[owner]] char *arr[4]; /* arrays of owners at file scope: not tracked */
extern [[owner]] char *ext;

void g_init(void) { if (!g) g = malloc(16); }
void g_set(const char *s) { free(g); g = strdup(s); }
void g_clear(void) { free(g); g = NULL; free(g); }
void g_grow(size_t n) { char *t = realloc(g, n); if (t) g = t; }
void g_grow2(size_t n) { g = realloc(g, n); }
size_t g_len(void) { return g ? strlen(g) : 0; }
void g_from_local(void) { [[owner]] char *x = malloc(4); free(g); g = x; }
void g_from_plain(void) { char *x = malloc(4); free(g); g = x; }
void g_move(void) { free(pub); pub = malloc(2); free(g); g = pub; pub = NULL; }
[[owner]] char *g_take(void) { [[owner]] char *t = g; g = NULL; return t; }
[[owner]] char *g_take2(void) { return g; }
void g_static_local(void) { [[owner]] static char *loc; free(loc); loc = malloc(2); }
static char *kept;
void keep(char *p) { free(kept); kept = p; }
void g_unknown(void) { g = malloc(1); keep(g); g = malloc(2); } /* keep() may change g */

int main(void)
{
    g_init(); g_set("abc"); g_grow(32); g_grow2(64);
    if (g_len() != 3) return 1;
    g_from_local(); g_from_plain(); g_move();
    free(g_take()); free(g_take2()); g = NULL;
    g_static_local(); g_static_local();
    g_unknown(); g_clear();
    free(pub); pub = NULL;
    free(kept); kept = NULL;
    return 0;
}
