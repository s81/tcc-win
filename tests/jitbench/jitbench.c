/*
 * jitbench - baseline measurements for the libtcc in-memory (JIT) path.
 * See docs/JIT-BASELINE.md for the commands and the recorded numbers.
 *
 * build:  win32\tcc.exe -I. tests\jitbench\jitbench.c win32\libtcc.dll -o jitbench.exe
 * run:    with win32\ on PATH (libtcc.dll locates lib\ and include\ next to itself)
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libtcc.h"

/* psapi.h is not shipped with tcc-win: declare what is needed */
typedef struct {
    DWORD cb, PageFaultCount;
    SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage,
        QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage,
        PagefileUsage, PeakPagefileUsage, PrivateUsage;
} PMC_EX;

static void mem(PMC_EX *m)
{
    static BOOL (WINAPI *gpmi)(HANDLE, PMC_EX *, DWORD);
    if (!gpmi)
        gpmi = (void *)GetProcAddress(LoadLibraryA("psapi.dll"), "GetProcessMemoryInfo");
    memset(m, 0, sizeof *m);
    m->cb = sizeof *m;
    gpmi(GetCurrentProcess(), m, sizeof *m);
}

static double now(void)
{
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (double)c.QuadPart / (double)f.QuadPart;
}

static int nb_err, quiet;
static void on_err(void *opaque, const char *msg)
{
    nb_err++;
    if (!quiet)
        fprintf(stderr, "tcc: %s\n", msg);
}

static TCCState *mk(void)
{
    TCCState *s = tcc_new();
    if (!s)
        fprintf(stderr, "tcc_new failed\n"), exit(2);
    tcc_set_error_func(s, NULL, on_err);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    return s;
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *p;
    if (!f)
        fprintf(stderr, "cannot open %s\n", path), exit(2);
    fseek(f, 0, SEEK_END), n = ftell(f), fseek(f, 0, SEEK_SET);
    p = malloc(n + 1);
    fread(p, 1, n, f), p[n] = 0;
    fclose(f);
    return p;
}

/* size of the VirtualAlloc'ed run image that contains 'addr' */
static void image_size(void *addr, size_t *total, size_t *exec)
{
    MEMORY_BASIC_INFORMATION mi;
    char *p, *base;
    *total = *exec = 0;
    if (!addr || !VirtualQuery(addr, &mi, sizeof mi))
        return;
    base = p = mi.AllocationBase;
    while (VirtualQuery(p, &mi, sizeof mi) && mi.AllocationBase == (void *)base) {
        *total += mi.RegionSize;
        if (mi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))
            *exec += mi.RegionSize;
        p += mi.RegionSize;
    }
}

/* ------------------------------------------------------------------ */
/* scale <file.c> [symbol] : one compile, timings + memory */
static int cmd_scale(int argc, char **argv)
{
    char *src = slurp(argv[0]);
    const char *sym = argc > 1 ? argv[1] : "f0";
    size_t lines = 0, tot, ex;
    double t0, t1, t2, t3, t4;
    int rc1, rc2 = -1;
    void *p = NULL;
    TCCState *s;
    PMC_EX m;

    for (char *q = src; *q; q++)
        lines += *q == '\n';
    t0 = now();
    s = mk();
    t1 = now();
    rc1 = tcc_compile_string(s, src);
    t2 = now();
    if (rc1 == 0)
        rc2 = tcc_relocate(s);
    t3 = now();
    if (rc2 == 0)
        p = tcc_get_symbol(s, sym);
    image_size(p, &tot, &ex);
    if (p && argc > 2)
        printf("call %s() = %d\n", sym, ((int (*)(void))p)());
    mem(&m);
    tcc_delete(s);
    t4 = now();
    printf("lines=%llu src_bytes=%llu rc_compile=%d rc_relocate=%d\n",
        (unsigned long long)lines, (unsigned long long)strlen(src), rc1, rc2);
    printf("ms: new=%.3f compile=%.3f relocate=%.3f delete=%.3f  compile_ms_per_kloc=%.3f\n",
        (t1 - t0) * 1e3, (t2 - t1) * 1e3, (t3 - t2) * 1e3, (t4 - t3) * 1e3,
        lines ? (t2 - t1) * 1e3 / (lines / 1000.0) : 0.0);
    printf("image_bytes=%llu exec_bytes=%llu peak_ws_kb=%llu peak_private_kb=%llu\n",
        (unsigned long long)tot, (unsigned long long)ex,
        (unsigned long long)m.PeakWorkingSetSize / 1024,
        (unsigned long long)m.PeakPagefileUsage / 1024);
    return rc1 || rc2;
}

/* ------------------------------------------------------------------ */
/* cycles <n> <ok|err|run> : memory growth over new/compile/(relocate|run)/delete */
static int cmd_cycles(int argc, char **argv)
{
    int n = atoi(argv[0]), i, bad = 0;
    const char *mode = argv[1];
    static const char ok_src[] =
        "int add(int a, int b) { return a + b; }\n"
        "int main(void) { return add(1, -1); }\n";
    static const char err_src[] =
        "int add(int a, int b) { return a + b; }\n"
        "int f(void) { return no_such_variable + add(1, 2); }\n"
        "int g(void) { return 1 +; }\n";
    PMC_EX m;
    DWORD h;
    double t0 = now();
    /* not in tcc-win's kernel32.def */
    BOOL (WINAPI *GetProcessHandleCount)(HANDLE, DWORD *) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcessHandleCount");

    quiet = 1;
    for (i = 0; i <= n; i++) {
        if (i % (n / 10 ? n / 10 : 1) == 0) {
            mem(&m);
            GetProcessHandleCount(GetCurrentProcess(), &h);
            printf("cycle %7d  ws_kb=%llu private_kb=%llu handles=%lu t=%.1fs\n", i,
                (unsigned long long)m.WorkingSetSize / 1024,
                (unsigned long long)m.PrivateUsage / 1024, h, now() - t0);
            fflush(stdout);
        }
        if (i == n)
            break;
        TCCState *s = mk();
        if (mode[0] == 'e') {
            if (tcc_compile_string(s, err_src) == 0)
                bad++;
        } else if (mode[0] == 'r') {
            if (tcc_compile_string(s, ok_src) || tcc_run(s, 0, NULL) != 0)
                bad++;
        } else {
            int (*add)(int, int);
            if (tcc_compile_string(s, ok_src) || tcc_relocate(s)
                || !(add = tcc_get_symbol(s, "add")) || add(40, 2) != 42)
                bad++;
        }
        tcc_delete(s);
    }
    printf("mode=%s cycles=%d unexpected=%d errors_reported=%d  %.1f us/cycle\n",
        mode, n, bad, nb_err, (now() - t0) * 1e6 / n);
    return bad != 0;
}

/* ------------------------------------------------------------------ */
/* threads <T> <K> <file.c> : T threads, each K x new/compile/relocate/delete */
static const char *th_src;
static int th_k;
static LONG th_bad;
static DWORD WINAPI th_main(void *arg)
{
    for (int i = 0; i < th_k; i++) {
        TCCState *s = mk();
        if (tcc_compile_string(s, th_src) || tcc_relocate(s))
            InterlockedIncrement(&th_bad);
        tcc_delete(s);
    }
    return 0;
}

static int cmd_threads(int argc, char **argv)
{
    int T = atoi(argv[0]), i;
    HANDLE h[64];
    FILETIME c, e, k, u;
    double t0, dt, cpu;

    th_k = atoi(argv[1]);
    th_src = slurp(argv[2]);
    if (T > 64)
        T = 64;
    t0 = now();
    for (i = 0; i < T; i++)
        h[i] = CreateThread(NULL, 0, th_main, NULL, 0, NULL);
    WaitForMultipleObjects(T, h, TRUE, INFINITE);
    dt = now() - t0;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    cpu = ((((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime)
         + (((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime)) / 1e7;
    printf("threads=%d compiles=%d bad=%ld wall_s=%.3f cpu_s=%.3f compiles_per_s=%.1f cpu/wall=%.2f\n",
        T, T * th_k, th_bad, dt, cpu, T * th_k / dt, cpu / dt);
    return th_bad != 0;
}

/* ------------------------------------------------------------------ */
/* kern <kernels.c> <name> <mib> <reps> <fill: b|d> [compiles]
   kernel signature: uint64_t k(const uint8_t *src, uint8_t *dst, size_t n) */
typedef uint64_t kern_fn(const uint8_t *, uint8_t *, size_t);

static int cmd_kern(int argc, char **argv)
{
    char *src = slurp(argv[0]), def[64];
    const char *name = argv[1];
    size_t n = (size_t)atoi(argv[2]) << 20, i;
    int reps = atoi(argv[3]), ncomp = argc > 5 ? atoi(argv[5]) : 200, r;
    uint8_t *a = VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    uint8_t *b = VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    uint64_t x = 0x9E3779B97F4A7C15ull, res = 0;
    double best = 1e30, t, tc = 0, tcmin = 1e30;
    TCCState *s = NULL;
    kern_fn *f;

    snprintf(def, sizeof def, "K_%s", name);
    if (argv[4][0] == 'd') {
        for (i = 0; i < n / 8; i++)
            ((double *)a)[i] = (double)(i % 1000003) * 0.001 + 0.1, ((double *)b)[i] = 1.0;
    } else {
        for (i = 0; i < n / 8; i++) { /* xorshift64, mostly text-like bytes */
            x ^= x << 13, x ^= x >> 7, x ^= x << 17;
            ((uint64_t *)a)[i] = (x & 0x5f5f5f5f5f5f5f5full) | 0x2020202020202020ull;
        }
        for (i = 0; i < n; i += 61)
            a[i] = "\n \t "[i % 4];
        memset(b, 0, n);
    }
    /* compile cost of this kernel as its own unit */
    for (r = 0; r <= ncomp; r++) {
        double t0 = now();
        s = mk();
        tcc_define_symbol(s, def, "1");
        if (tcc_compile_string(s, src) || tcc_relocate(s))
            return 1;
        t = now() - t0;
        if (r) /* first one warms file caches */
            tc += t, tcmin = t < tcmin ? t : tcmin;
        if (r < ncomp)
            tcc_delete(s);
    }
    snprintf(def, sizeof def, "k_%s", name);
    f = tcc_get_symbol(s, def);
    if (!f)
        return fprintf(stderr, "no symbol %s\n", def), 1;
    for (r = 0; r < reps; r++) {
        double t0 = now();
        res = f(a, b, n);
        t = now() - t0;
        best = t < best ? t : best;
    }
    for (i = 0, x = 0; i < n; i += 4099) /* cheap output digest */
        x = x * 31 + b[i];
    printf("%-10s n=%lluMiB best_ms=%.2f ns_per_byte=%.4f GiB_per_s=%.3f result=%016llx out=%016llx "
           "compile_ms_mean=%.4f compile_ms_min=%.4f\n",
        name, (unsigned long long)(n >> 20), best * 1e3, best * 1e9 / n,
        n / best / (1 << 30), (unsigned long long)res, (unsigned long long)x,
        tc * 1e3 / ncomp, tcmin * 1e3);
    tcc_delete(s);
    return 0;
}

/* ------------------------------------------------------------------ */
/* pattern byte at 64-bit offset o: depends on the high half so that
   32-bit truncated offsets read detectably wrong data */
static uint8_t pat(uint64_t o) { return (uint8_t)(o ^ (o >> 13) ^ ((o >> 32) * 7)); }

/* mkfile <path> <mib> : write the pattern file, print size and byte sum */
static int cmd_mkfile(int argc, char **argv)
{
    uint64_t size = (uint64_t)atoll(argv[1]) << 20, o, sum = 0;
    size_t bs = 1 << 20, i;
    uint8_t *buf = malloc(bs);
    HANDLE h = CreateFileA(argv[0], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    double t0 = now();
    DWORD w;
    if (h == INVALID_HANDLE_VALUE)
        return fprintf(stderr, "cannot create %s\n", argv[0]), 1;
    for (o = 0; o < size; o += bs) {
        for (i = 0; i < bs; i++)
            sum += buf[i] = pat(o + i);
        if (!WriteFile(h, buf, (DWORD)bs, &w, NULL) || w != bs)
            return fprintf(stderr, "write failed at %llu\n", (unsigned long long)o), 1;
    }
    CloseHandle(h);
    printf("wrote %llu bytes sum=%llu in %.1fs\n", (unsigned long long)size,
        (unsigned long long)sum, now() - t0);
    return 0;
}

/* stream <stream.c> <fn> <path> : JIT-compile stream.c, call
   int fn(const char *path, uint64_t out[8]) and report memory + throughput */
static int cmd_stream(int argc, char **argv)
{
    TCCState *s = mk();
    int (*fn)(const char *, uint64_t *), rc, i;
    uint64_t out[8] = {0};
    PMC_EX m0, m1;
    double t0;

    if (tcc_compile_string(s, slurp(argv[0])) || tcc_relocate(s)
        || !(fn = tcc_get_symbol(s, argv[1])))
        return fprintf(stderr, "compile/lookup failed\n"), 1;
    mem(&m0);
    t0 = now();
    rc = fn(argv[2], out);
    t0 = now() - t0;
    mem(&m1);
    printf("%s rc=%d seconds=%.2f", argv[1], rc, t0);
    for (i = 0; i < 8; i++)
        printf(" out%d=%llu", i, (unsigned long long)out[i]);
    printf("\n  GiB_per_s=%.3f ws_before_kb=%llu peak_ws_kb=%llu peak_private_kb=%llu\n",
        out[0] / t0 / (1 << 30), (unsigned long long)m0.WorkingSetSize / 1024,
        (unsigned long long)m1.PeakWorkingSetSize / 1024,
        (unsigned long long)m1.PeakPagefileUsage / 1024);
    tcc_delete(s);
    return rc;
}

/* ------------------------------------------------------------------ */
/* reloc <test> : relocation-range probes; each runs in its own process
   because the expected failure mode may be a crash */
static int host_fn(int x) { return x + 1000; }

static int cmd_reloc(int argc, char **argv)
{
    const char *t = argv[0];
    TCCState *s = mk();
    size_t tot, ex;
    void *p;

    if (!strcmp(t, "fardata") || !strcmp(t, "neardata")) {
        /* a host variable registered with tcc_add_symbol, far from / near the image */
        int *v = t[0] == 'f'
            ? VirtualAlloc((void *)0x00007E0000000000ull, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
            : malloc(sizeof *v);
        int (*get)(void);
        void (*set)(int);
        if (!v)
            return fprintf(stderr, "alloc failed\n"), 2;
        *v = 1234;
        tcc_add_symbol(s, "host_var", v);
        if (tcc_compile_string(s,
                /* without dllimport: "symbol 'host_var' is missing __declspec(dllimport)" */
                "extern __declspec(dllimport) int host_var;\n"
                "int get(void) { return host_var; }\n"
                "void set(int x) { host_var = x; }\n")
            || tcc_relocate(s))
            return printf("%s: compile/relocate FAILED (errors=%d)\n", t, nb_err), 1;
        get = tcc_get_symbol(s, "get"), set = tcc_get_symbol(s, "set");
        printf("%s: code=%p var=%p distance=%lld MiB\n", t, (void *)get, (void *)v,
            (long long)(((char *)v - (char *)get) / (1 << 20)));
        fflush(stdout);
        set(77);
        printf("%s: get()=%d *v=%d -> %s\n", t, get(), *v, get() == 77 && *v == 77 ? "OK" : "WRONG");
    } else if (!strcmp(t, "farfunc")) {
        int (*call)(int);
        /* x86-64 only: "lea eax,[rcx+1000]; ret" placed far above the image */
        static const unsigned char far_code[] = { 0x8D, 0x81, 0xE8, 0x03, 0x00, 0x00, 0xC3 };
        void *far_fn = VirtualAlloc((void *)0x00007E0000000000ull, 4096,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!far_fn)
            return fprintf(stderr, "alloc failed\n"), 2;
        memcpy(far_fn, far_code, sizeof far_code);
#define host_fn ((int (*)(int))far_fn)
        tcc_add_symbol(s, "host_fn", host_fn);
        if (tcc_compile_string(s,
                "int host_fn(int);\n#include <stdio.h>\n"
                "int call(int x) { char b[32]; sprintf(b, \"%d\", x); return host_fn(x); }\n"
                "void *addr_of_sprintf(void) { return (void*)sprintf; }\n")
            || tcc_relocate(s))
            return printf("%s: compile/relocate FAILED (errors=%d)\n", t, nb_err), 1;
        call = tcc_get_symbol(s, "call");
        p = ((void *(*)(void))tcc_get_symbol(s, "addr_of_sprintf"))();
        printf("%s: code=%p host_fn=%p (%lld MiB) msvcrt sprintf=%p (%lld MiB)\n", t,
            (void *)call, (void *)host_fn, (long long)(((char *)host_fn - (char *)call) / (1 << 20)),
            p, (long long)(((char *)p - (char *)call) / (1 << 20)));
        fflush(stdout);
        printf("%s: call(1)=%d -> %s\n", t, call(1), call(1) == 1001 ? "OK" : "WRONG");
#undef host_fn
    } else if (!strcmp(t, "bigarray") || !strcmp(t, "bigbss") || !strcmp(t, "bss1g")) {
        /* bigarray: one 3 GiB object; bigbss: 2 x 1.5 GiB (image > 2 GiB);
           bss1g: 2 x 512 MiB as a control */
        long long (*probe)(void);
        static const char one[] =
            "static char a[3ULL << 30];\n"
            "long long probe(void) { a[0] = 1; a[sizeof a - 1] = 2;\n"
            "  return (long long)sizeof a + a[0] + a[sizeof a - 1]; }\n";
        static const char two[] =
            "static char a[N], b[N];\n"
            "long long probe(void) { a[0] = 1; b[sizeof b - 1] = 2;\n"
            "  return (long long)sizeof a + sizeof b + a[0] + b[sizeof b - 1]; }\n";
        long long want = t[3] == 'a' ? (3LL << 30) + 3 : t[1] == 'i' ? (3000LL << 20) + 3 : (1024LL << 20) + 3;
        tcc_define_symbol(s, "N", t[1] == 'i' ? "(1500u << 20)" : "(512u << 20)");
        if (tcc_compile_string(s, t[3] == 'a' ? one : two) || tcc_relocate(s))
            return printf("%s: compile/relocate FAILED (errors=%d)\n", t, nb_err), 1;
        probe = tcc_get_symbol(s, "probe");
        image_size(probe, &tot, &ex);
        printf("%s: image_bytes=%llu\n", t, (unsigned long long)tot);
        fflush(stdout);
        printf("%s: probe()=%lld (expect %lld)\n", t, probe(), want);
    } else
        return fprintf(stderr, "unknown reloc test %s\n", t), 2;
    tcc_delete(s);
    return 0;
}

/* ------------------------------------------------------------------ */
/* outlive : is generated code still mapped after tcc_delete()? */
static int cmd_outlive(int argc, char **argv)
{
    TCCState *s = mk();
    MEMORY_BASIC_INFORMATION mi;
    void *f;
    if (tcc_compile_string(s, "int f(void) { return 42; }\n") || tcc_relocate(s))
        return 1;
    f = tcc_get_symbol(s, "f");
    VirtualQuery(f, &mi, sizeof mi);
    printf("before tcc_delete: %p state=%#lx protect=%#lx (0x40 = PAGE_EXECUTE_READWRITE)\n",
        f, mi.State, mi.Protect);
    tcc_delete(s);
    VirtualQuery(f, &mi, sizeof mi);
    printf("after  tcc_delete: %p state=%#lx (0x10000 = MEM_FREE) -> code %s\n",
        f, mi.State, mi.State == MEM_FREE ? "is unmapped with the TCCState" : "is still mapped");
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*fn)(int, char **); int min; } cmds[] = {
        { "scale", cmd_scale, 1 }, { "cycles", cmd_cycles, 2 }, { "threads", cmd_threads, 3 },
        { "kern", cmd_kern, 5 }, { "mkfile", cmd_mkfile, 2 }, { "stream", cmd_stream, 3 },
        { "reloc", cmd_reloc, 1 }, { "outlive", cmd_outlive, 0 },
    };
    for (int i = 0; argc > 1 && i < (int)(sizeof cmds / sizeof *cmds); i++)
        if (!strcmp(argv[1], cmds[i].name) && argc - 2 >= cmds[i].min)
            return cmds[i].fn(argc - 2, argv + 2);
    fprintf(stderr, "usage: jitbench scale|cycles|threads|kern|mkfile|stream|reloc|outlive ...\n");
    return 2;
}
