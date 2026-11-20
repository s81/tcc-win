/* __declspec(thread) means the same as _Thread_local. Windows only.
   DLL=1: a DLL (it calls one API so that it has an import table)
   otherwise: an EXE that links it and checks isolation per thread. */
#include <windows.h>

#ifdef DLL
__declspec(thread) int ds_dll = 9;
__declspec(thread) int ds_dll_zero;
__declspec(dllexport) int dll_get(int *z) { *z = ds_dll_zero; return ds_dll; }
__declspec(dllexport) void dll_set(int v)
{
    ds_dll = v;
    ds_dll_zero = v + 1;
    GetCurrentThreadId();
}
#else
#include <stdio.h>

__declspec(dllimport) int dll_get(int *);
__declspec(dllimport) void dll_set(int);

__declspec(thread) int ds_exe = 3;
__declspec(thread) int ds_exe_zero;
__declspec(thread) static int ds_exe_static = 4;
static __declspec(thread) char ds_buf[64];

static int bad;
#define CHK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); bad++; } } while (0)

static void check(int v)
{
    int i, z;
    CHK(ds_exe == 3 && ds_exe_zero == 0 && ds_exe_static == 4 && ds_buf[63] == 0);
    CHK(dll_get(&z) == 9 && z == 0);
    ds_exe = v; ds_exe_zero = v + 1; ds_exe_static = v + 2; ds_buf[63] = (char)v;
    dll_set(v);
    for (i = 0; i < 20000; i++)
        if (ds_exe != v || ds_exe_zero != v + 1 || ds_exe_static != v + 2
            || ds_buf[63] != (char)v || dll_get(&z) != v || z != v + 1) {
            CHK(0);
            break;
        }
}

static DWORD WINAPI worker(void *p)
{
    check((int)(INT_PTR)p);
    return 0;
}

int main(void)
{
    HANDLE h[4];
    int i, z;

    check(21);
    for (i = 0; i < 4; i++)
        h[i] = CreateThread(0, 0, worker, (void *)(INT_PTR)(10 + i), 0, 0);
    WaitForMultipleObjects(4, h, TRUE, INFINITE);
    CHK(ds_exe == 21 && ds_exe_zero == 22 && ds_exe_static == 23 && ds_buf[63] == 21);
    CHK(dll_get(&z) == 21 && z == 22);
    printf(bad ? "FAILED\n" : "declspec thread ok\n");
    return bad != 0;
}
#endif
