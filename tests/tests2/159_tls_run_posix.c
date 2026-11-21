/* Thread-local variables in code that is not loaded by the dynamic loader:
   tcc -run (this test runs that way). Each thread must start from the initial
   values and see only its own copy. */
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

static int global = 42;

_Thread_local int ti = 7;
_Thread_local int tz;
__thread double td = 2.5;
_Thread_local char tbuf[64] = "init";
_Thread_local struct { int a; long long b; } ts = { 11, 1LL << 40 };
/* over-aligned: the block of a thread must keep these alignments */
_Alignas(64) _Thread_local char al64[64] = "a64";
_Alignas(32) _Thread_local int al32 = 7;
_Thread_local int *tp = &global;        /* a relocation inside the template */
_Thread_local float tf[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
_Thread_local long long tl = 1LL << 50;

static int counter(void)
{
    static _Thread_local int n = 100;   /* function-static, per thread */
    return ++n;
}

static volatile int bad;
#define CHK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); bad = 1; } } while (0)

static int *addr[9];

static void *worker(void *arg)
{
    int id = (int)(long)arg, i;
    /* a new thread sees the initial values, not the main thread's */
    CHK(ti == 7 && tz == 0 && td == 2.5 && !strcmp(tbuf, "init"));
    CHK(ts.a == 11 && ts.b == 1LL << 40);
    CHK(((long)&al64 & 63) == 0 && ((long)&al32 & 31) == 0 && !strcmp(al64, "a64") && al32 == 7);
    CHK(*tp == 42 && tp == &global);
    CHK(tf[0] == 1.5f && tf[3] == 4.5f && tl == 1LL << 50);
    CHK(counter() == 101);
    addr[id] = &ti;
    for (i = 0; i < 1000; i++) {
        ti += id, tz -= id, td += 1.0, tf[1] += 1.0f, tl += id;
        sched_yield();   /* let the other threads interleave */
    }
    sprintf(tbuf, "thread %d", id);
    ts.a = id;
    CHK(ti == 7 + 1000 * id && tz == -1000 * id && td == 1002.5);
    CHK(tf[1] == 1002.5f && tl == (1LL << 50) + 1000LL * id);
    CHK(counter() == 102 && ts.a == id);
    CHK(ti + tz == 7 && (long long)(ti * td) == (long long)((7 + 1000 * id) * 1002.5));
    { char want[16]; sprintf(want, "thread %d", id); CHK(!strcmp(tbuf, want)); }
    return NULL;
}

int main(void)
{
    pthread_t h[8];
    int i, distinct = 1;

    CHK(ti == 7 && tz == 0 && td == 2.5 && !strcmp(tbuf, "init"));
    CHK(*tp == 42 && tf[2] == 3.5f && tl == 1LL << 50);
    ti = -1, tz = 5, td = 0.25, strcpy(tbuf, "main"), ts.a = -11;
    CHK(counter() == 101 && counter() == 102);
    addr[8] = &ti;

    for (i = 0; i < 8; i++)
        pthread_create(&h[i], NULL, worker, (void *)(long)i);
    for (i = 0; i < 8; i++)
        pthread_join(h[i], NULL);

    /* the threads did not touch the main thread's copies */
    CHK(ti == -1 && tz == 5 && td == 0.25 && !strcmp(tbuf, "main") && ts.a == -11);
    CHK(tf[1] == 2.5f && tl == 1LL << 50);
    CHK(counter() == 103);
    for (i = 0; i < 9; i++) {
        int j;
        for (j = 0; j < i; j++)
            if (addr[i] == addr[j])
                distinct = 0;
    }
    CHK(distinct);
    printf("%s\n", bad ? "tls run: FAILED" : "tls run: ok");
    return 0;
}
