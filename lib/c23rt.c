/* C23/C11 library functions that msvcrt.dll lacks (part of libtcc1.a on
   Windows targets):

   <string.h> memset_explicit, strndup
   <stdlib.h> memalignment, strfromd, strfromf, strfroml, aligned_alloc,
              free_sized, free_aligned_sized; __tcc_free, __tcc_realloc
              (free/realloc that also accept aligned_alloc blocks)
   <time.h>   gmtime_r, localtime_r, timegm, timespec_get, timespec_getres
   <math.h>   roundeven, nextup, nextdown, fmaximum, fminimum, fmaximum_num,
              fminimum_num, fmaximum_mag, fminimum_mag, fmaximum_mag_num,
              fminimum_mag_num (each also with f and l suffix)

   long double is double on these targets, so the l variants call the
   double ones.  The math and string parts use no Windows API and can be
   tested natively (build without _WIN32). */

#include <stddef.h>
#include <stdint.h>

void *malloc(size_t);
void free(void *);
void *realloc(void *, size_t);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);

/* ------------------------------------------------------------------ */
/* <string.h> */

void *memset_explicit(void *s, int c, size_t n)
{
    volatile unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *d = malloc(len + 1);
    if (d) {
        memcpy(d, s, len);
        d[len] = 0;
    }
    return d;
}

/* ------------------------------------------------------------------ */
/* <stdlib.h> */

size_t memalignment(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    return a & -a;
}

/* aligned_alloc: msvcrt's free cannot release _aligned_malloc memory, so
   aligned_alloc over-allocates with malloc and returns an aligned pointer
   inside the block, with a header {size, original pointer} just before it.
   In C23 mode <stdlib.h>/<malloc.h> bind free and realloc to __tcc_free and
   __tcc_realloc (asm labels), which recognise such blocks.  Recognition does
   not trust the header (the bytes before an ordinary malloc pointer belong
   to the heap): every live over-aligned block is kept in a hash set, and
   only pointers found there are treated as aligned blocks.  While no such
   block is live, free and realloc cost one extra load and branch.
   Alignments up to what malloc guarantees anyway are served by plain
   malloc and are not tracked.  See docs/C23-STATUS.md for what remains
   unsupported (memory released by code that does not use these wrappers). */

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#define C23_MALLOC_ALIGN 16
#else
#define C23_MALLOC_ALIGN 8
#endif

/* a spin lock (held only for a few table operations) */
#ifdef _WIN32
__declspec(dllimport) void __stdcall Sleep(unsigned long);
#define al_yield() Sleep(0)
#else
#define al_yield() ((void)0)
#endif
#ifndef __ATOMIC_ACQUIRE /* tcc defines these in <stdatomic.h> only */
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#endif
static int al_lock;

static void al_acquire(void)
{
    int one = 1, old;
    for (;;) {
        __atomic_exchange(&al_lock, &one, &old, __ATOMIC_ACQUIRE);
        if (!old)
            return;
        al_yield();
    }
}

static void al_release(void)
{
    int zero = 0;
    __atomic_store(&al_lock, &zero, __ATOMIC_RELEASE);
}

#define AL_TOMB ((void *)1)
static void **al_tab;               /* open addressing, linear probing */
static size_t al_cap, al_used, al_tombs;
static volatile size_t al_live;     /* number of live tracked blocks */

static size_t al_hash(const void *p)
{
    uintptr_t a = (uintptr_t)p / C23_MALLOC_ALIGN;
    return (size_t)(a * (uintptr_t)0x9E3779B97F4A7C15ull);
}

/* slot holding p, or else the slot where p would be inserted */
static size_t al_find(const void *p)
{
    size_t m = al_cap - 1, i = al_hash(p) & m, tomb = (size_t)-1;
    for (;; i = (i + 1) & m) {
        void *k = al_tab[i];
        if (k == p)
            return i;
        if (k == AL_TOMB) {
            if (tomb == (size_t)-1)
                tomb = i;
        } else if (!k) {
            return tomb != (size_t)-1 ? tomb : i;
        }
    }
}

/* call with the lock held; 0 when out of memory */
static int al_insert(void *p)
{
    size_t i;
    if ((al_used + al_tombs + 1) * 2 > al_cap) {
        size_t ncap = 16, j, ocap = al_cap;
        void **ntab, **otab = al_tab;
        while ((al_used + 1) * 4 > ncap)
            ncap *= 2;
        if (!(ntab = malloc(ncap * sizeof *ntab)))
            return 0;
        for (j = 0; j < ncap; j++)
            ntab[j] = 0;
        al_tab = ntab, al_cap = ncap, al_tombs = 0;
        for (j = 0; j < ocap; j++)
            if (otab[j] && otab[j] != AL_TOMB)
                al_tab[al_find(otab[j])] = otab[j];
        free(otab);
    }
    i = al_find(p);
    if (al_tab[i] == AL_TOMB)
        al_tombs--;
    al_tab[i] = p;
    al_live = ++al_used;
    return 1;
}

/* is p a live aligned_alloc block?  With del, also forget it. */
static int al_lookup(void *p, int del)
{
    int found = 0;
    if (!p || !al_live || ((uintptr_t)p & (2 * C23_MALLOC_ALIGN - 1)))
        return 0;
    al_acquire();
    if (al_cap) {
        size_t i = al_find(p);
        if (al_tab[i] == p) {
            found = 1;
            if (del) {
                al_tab[i] = AL_TOMB;
                al_tombs++;
                al_live = --al_used;
            }
        }
    }
    al_release();
    return found;
}

void *aligned_alloc(size_t alignment, size_t size)
{
    char *orig, *p;
    int ok;
    if (alignment == 0 || (alignment & (alignment - 1)))
        return 0;
    if (alignment <= C23_MALLOC_ALIGN)
        return malloc(size);
    if (size > (size_t)-1 - alignment)
        return 0;
    /* orig is C23_MALLOC_ALIGN aligned and alignment is larger, so
       p - orig is in [C23_MALLOC_ALIGN, alignment]: room for the header */
    if (!(orig = malloc(size + alignment)))
        return 0;
    p = (char *)(((uintptr_t)orig + alignment) & ~(uintptr_t)(alignment - 1));
    ((void **)p)[-1] = orig;
    ((size_t *)p)[-2] = size;
    al_acquire();
    ok = al_insert(p);
    al_release();
    if (!ok) {
        free(orig);
        return 0;
    }
    return p;
}

void __tcc_free(void *p)
{
    if (al_lookup(p, 1))
        p = ((void **)p)[-1];
    free(p);
}

void *__tcc_realloc(void *p, size_t n)
{
    void *q;
    size_t old;
    if (!al_lookup(p, 0))
        return realloc(p, n);
    /* an aligned block: the result need only be malloc-aligned */
    if (n == 0) {           /* like msvcrt's realloc(p, 0): free, NULL */
        __tcc_free(p);
        return 0;
    }
    if (!(q = malloc(n)))   /* the old block stays valid */
        return 0;
    old = ((size_t *)p)[-2];
    memcpy(q, p, old < n ? old : n);
    __tcc_free(p);
    return q;
}

void free_sized(void *p, size_t size)
{
    (void)size;
    __tcc_free(p);
}

void free_aligned_sized(void *p, size_t alignment, size_t size)
{
    (void)alignment, (void)size;
    __tcc_free(p);
}

#ifdef _WIN32
__declspec(dllimport) int _scprintf(const char *, ...);
__declspec(dllimport) int _snprintf(char *, size_t, const char *, ...);
#define c23_scprintf _scprintf
#define c23_snprintf _snprintf
#else
int snprintf(char *, size_t, const char *, ...);
#define c23_scprintf(f, v) snprintf(NULL, 0, f, v)
#define c23_snprintf snprintf
#endif

static int d_isnan(double x);
static int d_isinf(double x);

/* copy s to buf (snprintf rules), return strlen(s) */
static int put_str(char *buf, size_t n, const char *s)
{
    size_t i, len = 0;
    while (s[len])
        len++;
    if (n) {
        for (i = 0; i < len && i < n - 1; i++)
            buf[i] = s[i];
        buf[i] = 0;
    }
    return (int)len;
}

/* format must be "%[.precision]{a,A,e,E,f,F,g,G}" (C23 7.24.1.3) */
int strfromd(char *restrict s, size_t n, const char *restrict format, double fp)
{
    char fmt[32];
    const char *p = format;
    int k = 0, len, conv;

    if (*p++ != '%')
        return -1;
    fmt[k++] = '%';
    if (*p == '.') {
        fmt[k++] = *p++;
        while (*p >= '0' && *p <= '9') {
            if (k > 20)
                return -1;
            fmt[k++] = *p++;
        }
    }
    conv = *p++;
    if (*p)
        return -1;
    switch (conv) {
    case 'a': case 'A': case 'e': case 'E':
    case 'f': case 'g': case 'G':
        break;
    case 'F': /* not known to msvcrt */
        break;
    default:
        return -1;
    }
    /* msvcrt prints "1.#INF" and the like: do inf and nan here */
    if (d_isnan(fp) || d_isinf(fp)) {
        int up = conv >= 'A' && conv <= 'Z';
        const char *t = d_isnan(fp) ? (up ? "NAN" : "nan") : (up ? "INF" : "inf");
        char tmp[8];
        int i = 0;
        uint64_t b;
        memcpy(&b, &fp, sizeof b);
        if (b >> 63)
            tmp[i++] = '-';
        while (*t)
            tmp[i++] = *t++;
        tmp[i] = 0;
        return put_str(s, n, tmp);
    }
    fmt[k++] = conv == 'F' ? 'f' : (char)conv;
    fmt[k] = 0;
    len = c23_scprintf(fmt, fp);
    if (len < 0)
        return len;
    {
        /* msvcrt writes at least three exponent digits ("1.5e+003");
           C wants the fewest, but at least two for e/E (one for p/P) */
        char small[64], *t = (size_t)len < sizeof small ? small : malloc(len + 1);
        char *q;
        if (!t)
            return -1;
        c23_snprintf(t, len + 1, fmt, fp);
        t[len] = 0;
        for (q = t; *q; q++)
            if ((*q == 'e' || *q == 'E' || *q == 'p' || *q == 'P')
                && (q[1] == '+' || q[1] == '-')) {
                char *d = q + 2, *e = d;
                int min = (*q == 'p' || *q == 'P') ? 1 : 2;
                while (*e >= '0' && *e <= '9')
                    e++;
                while (e - d > min && *d == '0') {
                    memmove(d, d + 1, strlen(d));
                    e--;
                }
                break;
            }
        len = put_str(s, n, t);
        if (t != small)
            free(t);
    }
    return len;
}

int strfromf(char *restrict s, size_t n, const char *restrict format, float fp)
{
    return strfromd(s, n, format, fp);
}

int strfroml(char *restrict s, size_t n, const char *restrict format, long double fp)
{
    return strfromd(s, n, format, (double)fp);
}

/* ------------------------------------------------------------------ */
/* <math.h> */

static uint64_t d2u(double x) { uint64_t u; memcpy(&u, &x, 8); return u; }
static double u2d(uint64_t u) { double x; memcpy(&x, &u, 8); return x; }
static uint32_t f2u(float x) { uint32_t u; memcpy(&u, &x, 4); return u; }
static float u2f(uint32_t u) { float x; memcpy(&x, &u, 4); return x; }

static int d_isnan(double x) { return (d2u(x) << 1) > 0xffe0000000000000ull; }
static int d_isinf(double x) { return (d2u(x) << 1) == 0xffe0000000000000ull; }
static int d_neg(double x) { return (int)(d2u(x) >> 63); }
static int f_isnan(float x) { return (f2u(x) << 1) > 0xff000000u; }
static int f_neg(float x) { return (int)(f2u(x) >> 31); }

double roundeven(double x)
{
    uint64_t u = d2u(x), t;
    int e = (int)(u >> 52 & 0x7ff) - 1023;
    double r, diff;

    if (e >= 52)            /* integral, inf or nan */
        return d_isnan(x) ? x + x : x;
    if (e < 0) {
        t = u & 0x8000000000000000ull;      /* |x| < 1: trunc is +-0 */
    } else {
        t = u & ~(0x000fffffffffffffull >> e);
    }
    r = u2d(t);
    diff = x - r;           /* exact */
    if (diff < 0)
        diff = -diff;
    if (diff > 0.5 || (diff == 0.5 && ((e < 0) ? 0 : (int)(t >> (52 - e) & 1)))) {
        /* away from zero by one; for e < 0 the truncated value is 0 (even) */
        r = d_neg(x) ? r - 1.0 : r + 1.0;
    }
    return r;
}

float roundevenf(float x)
{
    uint32_t u = f2u(x), t;
    int e = (int)(u >> 23 & 0xff) - 127;
    float r, diff;

    if (e >= 23)
        return f_isnan(x) ? x + x : x;
    if (e < 0)
        t = u & 0x80000000u;
    else
        t = u & ~(0x007fffffu >> e);
    r = u2f(t);
    diff = x - r;
    if (diff < 0)
        diff = -diff;
    if (diff > 0.5f || (diff == 0.5f && ((e < 0) ? 0 : (int)(t >> (23 - e) & 1))))
        r = f_neg(x) ? r - 1.0f : r + 1.0f;
    return r;
}

long double roundevenl(long double x) { return roundeven((double)x); }

double nextup(double x)
{
    uint64_t u = d2u(x);
    if (d_isnan(x))
        return x + x;
    if (u == 0x7ff0000000000000ull)         /* +inf */
        return x;
    if ((u << 1) == 0)                      /* +-0 */
        return u2d(1);
    return u2d(u >> 63 ? u - 1 : u + 1);
}

float nextupf(float x)
{
    uint32_t u = f2u(x);
    if (f_isnan(x))
        return x + x;
    if (u == 0x7f800000u)
        return x;
    if ((u << 1) == 0)
        return u2f(1);
    return u2f(u >> 31 ? u - 1 : u + 1);
}

double nextdown(double x) { return -nextup(-x); }
float nextdownf(float x) { return -nextupf(-x); }
long double nextupl(long double x) { return nextup((double)x); }
long double nextdownl(long double x) { return nextdown((double)x); }

/* x > y with -0 < +0; no nans */
static int d_gt(double x, double y)
{
    if (x == y)
        return d_neg(y) && !d_neg(x);
    return x > y;
}

static int f_gt(float x, float y)
{
    if (x == y)
        return f_neg(y) && !f_neg(x);
    return x > y;
}

double fmaximum(double x, double y)
{
    if (d_isnan(x) || d_isnan(y))
        return x + y;
    return d_gt(x, y) ? x : y;
}

double fminimum(double x, double y)
{
    if (d_isnan(x) || d_isnan(y))
        return x + y;
    return d_gt(x, y) ? y : x;
}

double fmaximum_num(double x, double y)
{
    if (d_isnan(x))
        return d_isnan(y) ? x + y : y;
    if (d_isnan(y))
        return x;
    return d_gt(x, y) ? x : y;
}

double fminimum_num(double x, double y)
{
    if (d_isnan(x))
        return d_isnan(y) ? x + y : y;
    if (d_isnan(y))
        return x;
    return d_gt(x, y) ? y : x;
}

static double d_abs(double x) { return u2d(d2u(x) & 0x7fffffffffffffffull); }

double fmaximum_mag(double x, double y)
{
    double ax = d_abs(x), ay = d_abs(y);
    if (d_isnan(x) || d_isnan(y))
        return x + y;
    if (ax > ay)
        return x;
    if (ay > ax)
        return y;
    return fmaximum(x, y);
}

double fminimum_mag(double x, double y)
{
    double ax = d_abs(x), ay = d_abs(y);
    if (d_isnan(x) || d_isnan(y))
        return x + y;
    if (ax < ay)
        return x;
    if (ay < ax)
        return y;
    return fminimum(x, y);
}

double fmaximum_mag_num(double x, double y)
{
    if (d_isnan(x))
        return d_isnan(y) ? x + y : y;
    if (d_isnan(y))
        return x;
    return fmaximum_mag(x, y);
}

double fminimum_mag_num(double x, double y)
{
    if (d_isnan(x))
        return d_isnan(y) ? x + y : y;
    if (d_isnan(y))
        return x;
    return fminimum_mag(x, y);
}

float fmaximumf(float x, float y)
{
    if (f_isnan(x) || f_isnan(y))
        return x + y;
    return f_gt(x, y) ? x : y;
}

float fminimumf(float x, float y)
{
    if (f_isnan(x) || f_isnan(y))
        return x + y;
    return f_gt(x, y) ? y : x;
}

float fmaximum_numf(float x, float y)
{
    if (f_isnan(x))
        return f_isnan(y) ? x + y : y;
    if (f_isnan(y))
        return x;
    return f_gt(x, y) ? x : y;
}

float fminimum_numf(float x, float y)
{
    if (f_isnan(x))
        return f_isnan(y) ? x + y : y;
    if (f_isnan(y))
        return x;
    return f_gt(x, y) ? y : x;
}

static float f_abs(float x) { return u2f(f2u(x) & 0x7fffffffu); }

float fmaximum_magf(float x, float y)
{
    float ax = f_abs(x), ay = f_abs(y);
    if (f_isnan(x) || f_isnan(y))
        return x + y;
    if (ax > ay)
        return x;
    if (ay > ax)
        return y;
    return fmaximumf(x, y);
}

float fminimum_magf(float x, float y)
{
    float ax = f_abs(x), ay = f_abs(y);
    if (f_isnan(x) || f_isnan(y))
        return x + y;
    if (ax < ay)
        return x;
    if (ay < ax)
        return y;
    return fminimumf(x, y);
}

float fmaximum_mag_numf(float x, float y)
{
    if (f_isnan(x))
        return f_isnan(y) ? x + y : y;
    if (f_isnan(y))
        return x;
    return fmaximum_magf(x, y);
}

float fminimum_mag_numf(float x, float y)
{
    if (f_isnan(x))
        return f_isnan(y) ? x + y : y;
    if (f_isnan(y))
        return x;
    return fminimum_magf(x, y);
}

long double fmaximuml(long double x, long double y) { return fmaximum(x, y); }
long double fminimuml(long double x, long double y) { return fminimum(x, y); }
long double fmaximum_numl(long double x, long double y) { return fmaximum_num(x, y); }
long double fminimum_numl(long double x, long double y) { return fminimum_num(x, y); }
long double fmaximum_magl(long double x, long double y) { return fmaximum_mag(x, y); }
long double fminimum_magl(long double x, long double y) { return fminimum_mag(x, y); }
long double fmaximum_mag_numl(long double x, long double y) { return fmaximum_mag_num(x, y); }
long double fminimum_mag_numl(long double x, long double y) { return fminimum_mag_num(x, y); }

/* ------------------------------------------------------------------ */
/* <time.h> */

#ifdef _WIN32
#include <time.h>

/* also from <sec_api/time_s.h>, repeated in case it is not included */
__declspec(dllimport) int __cdecl _gmtime64_s(struct tm *, const __time64_t *);
__declspec(dllimport) int __cdecl _localtime64_s(struct tm *, const __time64_t *);
__declspec(dllimport) __time64_t __cdecl _mkgmtime64(struct tm *);

typedef struct { unsigned long lo, hi; } c23_filetime;
__declspec(dllimport) void * __stdcall GetModuleHandleA(const char *);
__declspec(dllimport) void * __stdcall GetProcAddress(void *, const char *);
__declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(c23_filetime *);

/* time_t is 32 bits on i386 (_USE_32BIT_TIME_T), 64 bits elsewhere */
struct tm *gmtime_r(const time_t *t, struct tm *tm)
{
    __time64_t t64 = *t;
    return _gmtime64_s(tm, &t64) ? NULL : tm;
}

struct tm *localtime_r(const time_t *t, struct tm *tm)
{
    __time64_t t64 = *t;
    return _localtime64_s(tm, &t64) ? NULL : tm;
}

time_t timegm(struct tm *tm)
{
    __time64_t t64 = _mkgmtime64(tm);
    time_t t = (time_t)t64;
    if (t64 == -1 || (__time64_t)t != t64)
        return (time_t)-1;
    return t;
}

int timespec_get(struct timespec *ts, int base)
{
    static void (__stdcall *get)(c23_filetime *);
    c23_filetime ft;
    uint64_t v;

    if (base != 1 /* TIME_UTC */)
        return 0;
    if (!get) {
        /* Windows 8+, else the 1-16 ms resolution of GetSystemTimeAsFileTime */
        void *k = GetModuleHandleA("kernel32.dll");
        void *f = k ? GetProcAddress(k, "GetSystemTimePreciseAsFileTime") : NULL;
        get = f ? (void (__stdcall *)(c23_filetime *))f : GetSystemTimeAsFileTime;
    }
    get(&ft);
    v = ((uint64_t)ft.hi << 32 | ft.lo) - 116444736000000000ull; /* 1601 -> 1970 */
    ts->tv_sec = (time_t)(v / 10000000);
    ts->tv_nsec = (long)(v % 10000000) * 100;
    return base;
}

int timespec_getres(struct timespec *ts, int base)
{
    if (base != 1 /* TIME_UTC */)
        return 0;
    if (ts) {
        ts->tv_sec = 0;
        ts->tv_nsec = 100;
    }
    return base;
}
#endif
