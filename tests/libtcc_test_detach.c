/*
 * libtcc: detached code (tcc_detach / tcc_code_symbol / tcc_code_free),
 * release of the run image, page protection, and no memory growth over
 * repeated compile/unload cycles, and rejection of images over 2 GiB.
 *
 * usage: libtcc_test_detach [-Bdir] [-Idir] [-Ldir] ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libtcc.h"
#ifdef _WIN32
# include <windows.h>
#else
# include <pthread.h>
# include <unistd.h>
#endif

/* thread-local variables in in-memory code: Windows, and ELF on x86_64, i386
   and arm64 Linux (tccrun.c) */
#if defined _WIN32 || (defined __linux__ && (defined __x86_64__ || defined __i386__ || defined __aarch64__))
# define HAVE_TLS_RUN 1
#endif

static int argc_, fails, nerr;
static char **argv_;
static char last_err[256];

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void on_err(void *quiet, const char *msg)
{
    nerr++;
    snprintf(last_err, sizeof last_err, "%s", msg);
    if (!quiet)
        fprintf(stderr, "%s\n", msg);
}

static TCCState *mk(int quiet, const char *opts)
{
    TCCState *s = tcc_new();
    int i;
    if (!s)
        fprintf(stderr, "tcc_new failed\n"), exit(1);
    tcc_set_error_func(s, quiet ? (void *)1 : NULL, on_err);
    for (i = 1; i < argc_; ++i) {
        char *a = argv_[i];
        if (a[0] == '-' && a[1] == 'B')
            tcc_set_lib_path(s, a + 2);
        else if (a[0] == '-' && a[1] == 'I')
            tcc_add_include_path(s, a + 2);
        else if (a[0] == '-' && a[1] == 'L')
            tcc_add_library_path(s, a + 2);
    }
    if (opts) /* before tcc_set_output_type(), as libtcc_test_mt.c does */
        tcc_set_options(s, opts);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    return s;
}

static const char prog[] =
    "int g = 7;\n"
    "int f(int x) { return x + g; }\n"
    "const char *msg(void) { return \"hello\"; }\n";

#ifdef _WIN32
static size_t private_bytes(void)
{
    struct { DWORD cb, faults; SIZE_T a, b, c, d, e, f, g, h, priv; } m;
    static BOOL (WINAPI *gpmi)(HANDLE, void *, DWORD);
    if (!gpmi)
        gpmi = (void *)GetProcAddress(LoadLibraryA("psapi.dll"), "GetProcessMemoryInfo");
    memset(&m, 0, sizeof m);
    m.cb = sizeof m;
    gpmi(GetCurrentProcess(), &m, sizeof m);
    return m.priv;
}

static DWORD protect_of(void *p)
{
    MEMORY_BASIC_INFORMATION mi;
    VirtualQuery(p, &mi, sizeof mi);
    return mi.State == MEM_FREE ? 0 : mi.Protect;
}
#endif

#ifdef HAVE_TLS_RUN
# ifndef _WIN32
/* resident memory: a leaked 64 KB block per cycle shows up here */
static size_t private_bytes(void)
{
    long pages = 0, resident = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        if (fscanf(f, "%ld %ld", &pages, &resident) != 2)
            resident = 0;
        fclose(f);
    }
    return (size_t)resident * (size_t)sysconf(_SC_PAGESIZE);
}
# endif

static int (*tls_bump)(int);
static volatile int tls_result;

# ifdef _WIN32
static DWORD WINAPI tls_thread(LPVOID arg)
{
    tls_result = tls_bump((int)(INT_PTR)arg);
    return 0;
}
# else
static void *tls_thread(void *arg)
{
    tls_result = tls_bump((int)(long)arg);
    return NULL;
}
# endif

/* run tls_bump(d) in a new thread and wait for it: tls_result */
static void run_tls_thread(int d)
{
# ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, tls_thread, (LPVOID)(INT_PTR)d, 0, NULL);
    WaitForSingleObject(h, INFINITE), CloseHandle(h);
# else
    pthread_t t;
    pthread_create(&t, NULL, tls_thread, (void *)(long)d);
    pthread_join(t, NULL);
# endif
}
#endif

/* one compile + relocate, then unload by tcc_delete (mode 0) or detach (mode 1) */
static int cycle(int mode)
{
    TCCState *s = mk(0, NULL);
    TCCCode *c;
    int (*f)(int), ok;
    if (tcc_compile_string(s, prog) || tcc_relocate(s))
        return 0;
    if (mode == 0) {
        f = (int (*)(int))tcc_get_symbol(s, "f");
        ok = f && f(1) == 8;
        tcc_delete(s);
        return ok;
    }
    c = tcc_detach(s);
    tcc_delete(s);
    f = c ? (int (*)(int))tcc_code_symbol(c, "f") : NULL;
    ok = f && f(1) == 8;
    tcc_code_free(c);
    return ok;
}

int main(int argc, char **argv)
{
    TCCState *s;
    TCCCode *c;
    int (*f)(int), *g, mode, i;
    const char *(*msg)(void);

    argc_ = argc, argv_ = argv;

    /* 1. code survives tcc_delete after tcc_detach, and is freed by tcc_code_free */
    s = mk(0, NULL);
    CHECK(tcc_compile_string(s, prog) == 0);
    CHECK(tcc_relocate(s) == 0);
    c = tcc_detach(s);
    CHECK(c != NULL);
    CHECK(tcc_get_symbol(s, "f") == NULL); /* the state no longer owns the code */
    tcc_delete(s);
    if (c) {
        f = (int (*)(int))tcc_code_symbol(c, "f");
        g = (int *)tcc_code_symbol(c, "g");
        msg = (const char *(*)(void))tcc_code_symbol(c, "msg");
        CHECK(f && f(1) == 8);
        CHECK(g && *g == 7);
        CHECK(msg && strcmp(msg(), "hello") == 0);
        CHECK(tcc_code_symbol(c, "nosuch") == NULL);
        CHECK(tcc_code_symbol(c, "") == NULL);
#ifdef _WIN32
        /* W^X: code is execute-read, read-only data is read-only */
        CHECK(f && protect_of((void *)f) == PAGE_EXECUTE_READ);
        CHECK(msg && protect_of((void *)msg()) == PAGE_READONLY);
        CHECK(g && protect_of(g) == PAGE_READWRITE);
#endif
        tcc_code_free(c);
#ifdef _WIN32
        CHECK(f && protect_of((void *)f) == 0); /* MEM_FREE */
#endif
    }
    tcc_code_free(NULL); /* allowed, like free(NULL) */

    /* 2. without detach, tcc_delete releases the image */
    s = mk(0, NULL);
    CHECK(tcc_compile_string(s, prog) == 0);
    CHECK(tcc_relocate(s) == 0);
    f = (int (*)(int))tcc_get_symbol(s, "f");
    CHECK(f && f(2) == 9);
    tcc_delete(s);
#ifdef _WIN32
    CHECK(f && protect_of((void *)f) == 0);
#endif

    /* 3. detach before relocation is refused */
    s = mk(1, NULL);
    CHECK(tcc_compile_string(s, prog) == 0);
    nerr = 0;
    CHECK(tcc_detach(s) == NULL);
    CHECK(nerr == 1);
    tcc_delete(s);

    /* 4. detach with -bt is refused: backtraces need the state's symbol table */
    s = mk(1, "-bt");
    CHECK(tcc_compile_string(s, prog) == 0);
    CHECK(tcc_relocate(s) == 0);
    nerr = 0;
    CHECK(tcc_detach(s) == NULL);
    CHECK(nerr == 1);
    f = (int (*)(int))tcc_get_symbol(s, "f");
    CHECK(f && f(3) == 10); /* the state still owns working code */
    tcc_delete(s);

    /* 5. no growth over repeated cycles, with and without detach */
    for (mode = 0; mode < 2; mode++) {
#ifdef _WIN32
        size_t before = 0, grown;
#endif
        int bad = 0;
        for (i = 0; i < 6000; i++) {
#ifdef _WIN32
            if (i == 4000)
                before = private_bytes(); /* the heap settles within ~4000 cycles */
#endif
            bad += !cycle(mode);
        }
        CHECK(bad == 0);
#ifdef _WIN32
        grown = private_bytes();
        grown = grown > before ? grown - before : 0;
        printf("mode %s: growth over 2000 cycles: %u KB\n",
            mode ? "detach" : "delete", (unsigned)(grown / 1024));
        CHECK(grown < 1024 * 1024); /* the old leak was 4 KB per cycle */
#endif
    }

    /* 6. images over 2 GiB are rejected with a message, not silently */
    {
        static const char *big[] = {
            /* static: bss, rejected while compiling */
            "static char a[1500u << 20], b[1500u << 20];\n"
            "int f(void) { return a[0] + b[0]; }\n",
            /* -fcommon: placed in bss while linking; 4.5 GiB would wrap 32 bits */
            "char a[1500u << 20], b[1500u << 20], c[1500u << 20];\n"
            "int f(void) { return a[0] + b[0] + c[0]; }\n",
            /* two sections under 2 GiB each: caught when sizing the run image */
            "static char a[1500u << 20];\n"
            "__attribute__((section(\".bss2\"))) static char b[1500u << 20];\n"
            "int f(void) { return a[0] + b[0]; }\n",
        };
        for (i = 0; i < 3; i++) {
            int rc;
            s = mk(1, i == 1 ? "-fcommon" : NULL);
            nerr = 0, last_err[0] = 0;
            rc = tcc_compile_string(s, big[i]);
            if (rc == 0)
                rc = tcc_relocate(s);
            CHECK(rc == -1);
            CHECK(nerr >= 1 && strstr(last_err, "2 GiB"));
            printf("big image %d: %s\n", i, last_err);
            tcc_delete(s);
        }
    }

#ifdef HAVE_TLS_RUN
    /* 7. thread-local variables in in-memory code: every thread starts from
       the initial value, and the per-thread blocks (64 KB each here) are
       freed when a thread exits and when the code is unloaded */
    {
        static const char tls_prog[] =
            "_Thread_local int t = 5;\n"
            "_Thread_local char pad[65536];\n"
            "int bump(int d) { pad[d & 0xffff] = 1; t += d; return t; }\n";
        size_t before = 0, grown;
        int bad = 0;

        s = mk(0, NULL);
        CHECK(tcc_compile_string(s, tls_prog) == 0);
        CHECK(tcc_relocate(s) == 0);
        c = tcc_detach(s);
        tcc_delete(s);
        tls_bump = c ? (int (*)(int))tcc_code_symbol(c, "bump") : NULL;
        CHECK(tls_bump && tls_bump(1) == 6 && tls_bump(1) == 7);
        if (tls_bump) {
            run_tls_thread(10);
            CHECK(tls_result == 15);           /* a fresh copy: 5 + 10 */
            CHECK(tls_bump(0) == 7);           /* the main thread's copy is unchanged */
        }
        tcc_code_free(c);

        for (i = 0; i < 1500; i++) {
            if (i == 500)
                before = private_bytes();
            s = mk(0, NULL);
            tls_bump = NULL;
            if (tcc_compile_string(s, tls_prog) == 0 && tcc_relocate(s) == 0)
                tls_bump = (int (*)(int))tcc_get_symbol(s, "bump");
            if (!tls_bump || tls_bump(2) != 7) {
                bad++;
            } else {
                run_tls_thread(3);
                bad += tls_result != 8;
            }
            tcc_delete(s);                     /* frees the main thread's block */
        }
        CHECK(bad == 0);
        grown = private_bytes();
        grown = grown > before ? grown - before : 0;
        printf("tls: growth over 1000 cycles: %u KB\n", (unsigned)(grown / 1024));
        /* a leaked block per cycle is 64 MB; Linux measures resident memory,
           which is noisier */
#ifdef _WIN32
        CHECK(grown < 4 * 1024 * 1024);
#else
        CHECK(grown < 16 * 1024 * 1024);
#endif
    }
#endif

    if (fails)
        printf("%d check(s) failed\n", fails);
    else
        printf("libtcc_test_detach: all checks passed\n");
    return fails != 0;
}
