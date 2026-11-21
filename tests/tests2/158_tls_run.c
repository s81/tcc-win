/* Thread-local variables in code that is not loaded by the Windows loader:
   tcc -run (this test runs that way) and libtcc in-memory code. Each thread
   must start from the initial values and see only its own copy. */
#include <stdio.h>
#include <string.h>
#include <windows.h>

_Thread_local int ti = 7;
_Thread_local int tz;
__declspec(thread) double td = 2.5;
_Thread_local char tbuf[64] = "init";
_Thread_local struct { int a; long long b; } ts = { 11, 1LL << 40 };

static int counter(void)
{
    static _Thread_local int n = 100;   /* function-static, per thread */
    return ++n;
}

/* only ever set to 1, so no atomic is needed (InterlockedIncrement is an
   intrinsic, not a kernel32 export, on arm64) */
static volatile int bad;
#define CHK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); bad = 1; } } while (0)

static int *addr[9];

static DWORD WINAPI worker(LPVOID arg)
{
    int id = (int)(INT_PTR)arg, i;
    /* a new thread sees the initial values, not the main thread's */
    CHK(ti == 7 && tz == 0 && td == 2.5 && !strcmp(tbuf, "init"));
    CHK(ts.a == 11 && ts.b == 1LL << 40);
    CHK(counter() == 101);
    addr[id] = &ti;
    for (i = 0; i < 1000; i++) {
        ti += id, tz -= id, td += 1.0;
        Sleep(0);   /* let the other threads interleave */
    }
    sprintf(tbuf, "thread %d", id);
    ts.a = id;
    CHK(ti == 7 + 1000 * id && tz == -1000 * id && td == 1002.5);
    CHK(counter() == 102 && ts.a == id);
    { char want[16]; sprintf(want, "thread %d", id); CHK(!strcmp(tbuf, want)); }
    return 0;
}

int main(void)
{
    HANDLE h[8];
    int i, distinct = 1;

    CHK(ti == 7 && tz == 0 && td == 2.5 && !strcmp(tbuf, "init"));
    ti = -1, tz = 5, td = 0.25, strcpy(tbuf, "main"), ts.a = -11;
    CHK(counter() == 101 && counter() == 102);
    addr[8] = &ti;

    for (i = 0; i < 8; i++)
        h[i] = CreateThread(NULL, 0, worker, (LPVOID)(INT_PTR)i, 0, NULL);
    WaitForMultipleObjects(8, h, TRUE, INFINITE);
    for (i = 0; i < 8; i++)
        CloseHandle(h[i]);

    /* the threads did not touch the main thread's copies */
    CHK(ti == -1 && tz == 5 && td == 0.25 && !strcmp(tbuf, "main") && ts.a == -11);
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
