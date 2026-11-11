/* C11 aligned_alloc and C23 free_sized, free_aligned_sized (lib/c23rt.c).
   aligned_alloc memory must be accepted by plain free and realloc, and
   ordinary malloc memory must still work with them. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* free taken as a function pointer must be the wrapper too */
static void (*volatile free_fn)(void *) = free;

int main(void)
{
    static const size_t al[] = { 1, 2, 4, 8, 16, 32, 64, 128, 4096, 65536 };
    void *keep[64];
    size_t i, k;
    char *p, *q;

    for (i = 0; i < sizeof al / sizeof al[0]; i++) {
        p = aligned_alloc(al[i], 100);
        CHECK(p != NULL);
        CHECK((uintptr_t)p % al[i] == 0);
        CHECK(memalignment(p) >= al[i]);
        memset(p, 0x5a, 100);
        free(p);
    }

    /* invalid alignment */
    CHECK(aligned_alloc(0, 16) == NULL);
    CHECK(aligned_alloc(24, 48) == NULL);
    CHECK(aligned_alloc(64, SIZE_MAX - 8) == NULL);

    /* many live blocks, mixed with ordinary ones (grows the block set) */
    for (k = 0; k < 64; k++) {
        keep[k] = (k & 1) ? malloc(48 + k) : aligned_alloc(64 << (k % 4), 48 + k);
        CHECK(keep[k] != NULL);
        memset(keep[k], (int)k, 48 + k);
    }
    for (k = 0; k < 64; k += 3) {
        free(keep[k]);
        keep[k] = NULL;
    }
    for (k = 0; k < 64; k++)
        if (keep[k]) {
            CHECK(((unsigned char *)keep[k])[47] == (unsigned char)k);
            if (k % 3 == 1)
                free_sized(keep[k], 48 + k);
            else if (k & 1)
                free(keep[k]);
            else
                free_aligned_sized(keep[k], 64 << (k % 4), 48 + k);
        }

    /* realloc of an aligned block keeps the contents */
    p = aligned_alloc(256, 10);
    CHECK(p && (uintptr_t)p % 256 == 0);
    strcpy(p, "aligned");
    q = realloc(p, 1000);
    CHECK(q && strcmp(q, "aligned") == 0);
    q[999] = 1;
    p = realloc(q, 4);   /* now an ordinary block */
    CHECK(p && memcmp(p, "alig", 4) == 0);
    free(p);

    /* ordinary blocks with free/realloc, also while aligned blocks live */
    q = aligned_alloc(128, 32);
    p = malloc(16);
    strcpy(p, "plain");
    p = realloc(p, 4096);
    CHECK(p && strcmp(p, "plain") == 0);
    free(p);
    free(NULL);
    free_fn(q);
    free_sized(NULL, 0);
    free_aligned_sized(NULL, 64, 0);

    if (fails)
        return 1;
    printf("c23_aligned ok\n");
    return 0;
}
