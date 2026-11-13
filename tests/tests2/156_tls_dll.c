/* Thread-local storage in an import-less DLL. Windows only.
   DLL=1: a DLL that contains only TLS and calls nothing (it has no imports,
          so the PE writer must add one or the loader skips its TLS directory).
   otherwise: an EXE that loads that DLL implicitly (156a1.dll) and with
          LoadLibrary (156a2.dll) and checks the values per thread. */
#include <windows.h>

#ifdef DLL
typedef struct { int a; void *p; char buf[4096]; } Big;
_Thread_local int tl_init = 5;
_Thread_local int tl_zero;
_Thread_local Big tl_big;
__declspec(dllexport) int get_init(void) { return tl_init; }
__declspec(dllexport) int get_zero(void) { return tl_zero; }
__declspec(dllexport) void set_all(int v)
{
    tl_init = v; tl_zero = v;
    tl_big.a = v; tl_big.p = &tl_zero; tl_big.buf[4095] = (char)v;
}
__declspec(dllexport) int get_big(void)
{
    return tl_big.a + tl_big.buf[4095] + (tl_big.p == &tl_zero ? 1000 : 0);
}
#else
#include <stdio.h>

__declspec(dllimport) int get_init(void);
__declspec(dllimport) int get_zero(void);
__declspec(dllimport) void set_all(int);
__declspec(dllimport) int get_big(void);

typedef struct { int (*init)(void), (*zero)(void), (*big)(void); void (*set)(int); } Api;
static Api imp, dyn;
static HANDLE gate;
static int bad;
#define CHK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); bad++; } } while (0)

/* fresh thread: initial values, then set v and check it stays v and private */
static void check(Api *a, int v)
{
    int i;
    CHK(a->init() == 5 && a->zero() == 0 && a->big() == 0);
    a->set(v);
    for (i = 0; i < 20000; i++)
        if (a->init() != v || a->zero() != v
            || a->big() != 2 * v + 1000) {
            CHK(0);
            break;
        }
}

static DWORD WINAPI worker(void *p)
{
    int v = (int)(INT_PTR)p;
    check(&imp, v);
    check(&dyn, v + 100);
    return 0;
}

static DWORD WINAPI early(void *p)  /* started before LoadLibrary */
{
    WaitForSingleObject(gate, INFINITE);
    check(&dyn, 55);
    return 0;
}

int main(void)
{
    HANDLE h[4], e;
    HMODULE m;
    int i;

    imp.init = get_init; imp.zero = get_zero;
    imp.big = get_big; imp.set = set_all;
    gate = CreateEvent(0, TRUE, FALSE, 0);
    e = CreateThread(0, 0, early, 0, 0, 0);
    m = LoadLibraryA("156a2.dll");
    CHK(m != 0);
    if (!m)
        return 1;
    dyn.init = (void *)GetProcAddress(m, "get_init");
    dyn.zero = (void *)GetProcAddress(m, "get_zero");
    dyn.big = (void *)GetProcAddress(m, "get_big");
    dyn.set = (void *)GetProcAddress(m, "set_all");
    SetEvent(gate);
    check(&imp, 21);   /* main thread, both DLLs */
    check(&dyn, 22);
    for (i = 0; i < 4; i++)
        h[i] = CreateThread(0, 0, worker, (void *)(INT_PTR)(10 + i), 0, 0);
    WaitForMultipleObjects(4, h, TRUE, INFINITE);
    WaitForSingleObject(e, INFINITE);
    /* the main thread's values are untouched by the other threads */
    CHK(imp.init() == 21 && dyn.init() == 22);
    FreeLibrary(m);
    printf(bad ? "FAILED\n" : "tls ok\n");
    return bad != 0;
}
#endif
