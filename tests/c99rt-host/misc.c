#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { long long quot, rem; } lld_t;
lld_t lldiv(long long, long long);
lld_t imaxdiv(long long, long long);
int isblank(int); int iswblank(unsigned);
typedef struct { volatile long state; } once_t;
void call_once(once_t *, void (*)(void));
int at_quick_exit(void (*)(void));
void quick_exit(int);

static int calls;
static void once_fn(void) { calls++; }
static void ha(void) { puts("handler A"); fflush(stdout); }
static void hb(void) { puts("handler B"); fflush(stdout); }

int main(void)
{
    int fails = 0;
    once_t f = { 0 };
    lld_t d = lldiv(-7, 2), m = imaxdiv(7, -2);
    call_once(&f, once_fn); call_once(&f, once_fn); call_once(&f, once_fn);
    if (calls != 1) puts("FAIL call_once"), fails++;
    if (d.quot != -3 || d.rem != -1 || m.quot != -3 || m.rem != 1) puts("FAIL lldiv"), fails++;
    if (!isblank(' ') || !isblank('\t') || isblank('\n') || isblank('a')) puts("FAIL isblank"), fails++;
    if (!iswblank(' ') || !iswblank('\t') || iswblank('x')) puts("FAIL iswblank"), fails++;
    if (at_quick_exit(ha) || at_quick_exit(hb)) puts("FAIL at_quick_exit"), fails++;
    puts(fails ? "FAILED" : "misc ok");
    fflush(stdout);
    quick_exit(fails ? 1 : 3); /* handlers in reverse order, then exit status 3 */
}
