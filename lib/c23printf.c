/* C23 printf family for Windows targets (part of libtcc1.a).

   msvcrt.dll's printf lacks the C23 %b/%B conversions, prints three
   exponent digits (1.5e+003) and writes infinities/NaNs as 1.#INF.
   In C23 mode <stdio.h> maps printf, fprintf, sprintf, snprintf, vprintf,
   vfprintf, vsprintf and vsnprintf to the __tcc_ versions below (opt out
   with -D__TCC_MSVCRT_PRINTF).

   __tcc_vsnprintf walks the format.  %b/%B, %n, %% and non-finite floats
   are formatted here; every other conversion is handed to msvcrt's
   _snprintf one specification at a time, with '*' resolved and integer
   arguments widened to 64 bits (I64), and the exponent of e/E/g/G output
   is then cut to at least two digits.

   This file must not include <stdio.h>: the wrappers call msvcrt directly.
   Without _WIN32 it builds against the host C library for unit tests; the
   msvcrt exponent style is then simulated. */

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

void *malloc(size_t);
void free(void *);
size_t strlen(const char *);
void *memcpy(void *, const void *, size_t);

#ifdef _WIN32
typedef struct _iobuf FILE_;
__declspec(dllimport) int _snprintf(char *, size_t, const char *, ...);
__declspec(dllimport) int _scprintf(const char *, ...);
__declspec(dllimport) size_t fwrite(const void *, size_t, size_t, FILE_ *);
__declspec(dllimport) char *__iob_func(void);
#ifdef _WIN64
#define STDOUT_ ((FILE_ *)(__iob_func() + 48))
#else
#define STDOUT_ ((FILE_ *)(__iob_func() + 32))
#endif
#define LL_ "I64"
#define ms_snprintf _snprintf
#define ms_scprintf _scprintf
#else
/* host build for testing */
#include <stdio.h>
typedef FILE FILE_;
#define STDOUT_ stdout
#define LL_ "ll"
static int ms_vfmt(char *buf, size_t n, const char *f, va_list ap)
{
    /* like msvcrt: -1 when truncated, three exponent digits */
    char tmp[4096], *e;
    int len = vsnprintf(tmp, sizeof tmp, f, ap), i;
    char c = f[strlen(f) - 1];
    if (c == 'e' || c == 'E' || c == 'g' || c == 'G') {
        for (e = tmp; *e; e++)
            if ((*e == 'e' || *e == 'E') && (e[1] == '+' || e[1] == '-')
                && e[2] && e[3] && !e[4]) {
                for (i = len; i >= e + 2 - tmp; i--)
                    tmp[i + 1] = tmp[i];
                e[2] = '0', len++;
                break;
            }
    }
    if (buf == NULL)
        return len;
    if ((size_t)len > n)
        return -1;
    memcpy(buf, tmp, len < (int)n ? len + 1 : len);
    return len;
}
static int ms_snprintf(char *buf, size_t n, const char *f, ...)
{
    va_list ap; int r;
    va_start(ap, f); r = ms_vfmt(buf, n, f, ap); va_end(ap);
    return r;
}
static int ms_scprintf(const char *f, ...)
{
    va_list ap; int r;
    va_start(ap, f); r = ms_vfmt(NULL, 0, f, ap); va_end(ap);
    return r;
}
#endif

typedef struct {
    char *buf;
    size_t n;       /* buffer size */
    size_t len;     /* total output length so far */
} out_t;

static void put(out_t *o, const char *s, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++, o->len++)
        if (o->len + 1 < o->n)
            o->buf[o->len] = s[i];
}

static void pad(out_t *o, int c, int k)
{
    char ch = (char)c;
    while (k-- > 0)
        put(o, &ch, 1);
}

/* write s (length len) into width w; zpos: where zeros go for the 0 flag
   (-1: no zero padding) */
static void put_padded(out_t *o, const char *s, int len, int w, int left, int zpos)
{
    int k = w > len ? w - len : 0;
    if (left) {
        put(o, s, len), pad(o, ' ', k);
    } else if (zpos >= 0) {
        put(o, s, zpos), pad(o, '0', k), put(o, s + zpos, len - zpos);
    } else {
        pad(o, ' ', k), put(o, s, len);
    }
}

/* length modifiers */
enum { L_NONE, L_HH, L_H, L_L, L_LL, L_J, L_Z, L_T, L_BIGL, L_W8, L_W16, L_W32, L_W64 };

static uint64_t get_uint(va_list *ap, int lm)
{
    switch (lm) {
    case L_HH: case L_W8: return (unsigned char)va_arg(*ap, int);
    case L_H: case L_W16: return (unsigned short)va_arg(*ap, int);
    case L_L: return va_arg(*ap, unsigned long);
    case L_LL: case L_BIGL: case L_W64: return va_arg(*ap, unsigned long long);
    case L_J: return va_arg(*ap, uintmax_t);
    case L_Z: return va_arg(*ap, size_t);
    case L_T: return (size_t)va_arg(*ap, ptrdiff_t);
    default: return va_arg(*ap, unsigned int);
    }
}

static int64_t get_int(va_list *ap, int lm)
{
    switch (lm) {
    case L_HH: case L_W8: return (signed char)va_arg(*ap, int);
    case L_H: case L_W16: return (short)va_arg(*ap, int);
    case L_L: return va_arg(*ap, long);
    case L_LL: case L_BIGL: case L_W64: return va_arg(*ap, long long);
    case L_J: return va_arg(*ap, intmax_t);
    case L_Z: return (ptrdiff_t)va_arg(*ap, size_t);
    case L_T: return va_arg(*ap, ptrdiff_t);
    default: return va_arg(*ap, int);
    }
}

static void store_n(va_list *ap, int lm, size_t v)
{
    switch (lm) {
    case L_HH: case L_W8: *va_arg(*ap, signed char *) = (signed char)v; break;
    case L_H: case L_W16: *va_arg(*ap, short *) = (short)v; break;
    case L_L: *va_arg(*ap, long *) = (long)v; break;
    case L_LL: case L_BIGL: case L_W64: *va_arg(*ap, long long *) = (long long)v; break;
    case L_J: *va_arg(*ap, intmax_t *) = (intmax_t)v; break;
    case L_Z: *va_arg(*ap, size_t *) = v; break;
    case L_T: *va_arg(*ap, ptrdiff_t *) = (ptrdiff_t)v; break;
    default: *va_arg(*ap, int *) = (int)v; break;
    }
}

static int put_num(char *p, int v)
{
    char t[12];
    int k = 0, n = 0;
    do t[k++] = (char)('0' + v % 10); while (v /= 10);
    while (k)
        p[n++] = t[--k];
    return n;
}

/* cut "e+0dd" to "e+dd" (msvcrt always prints three digits) */
static int fix_exp(char *s, int len)
{
    int i, j;
    for (i = 0; i < len; i++) {
        if ((s[i] == 'e' || s[i] == 'E') && (s[i + 1] == '+' || s[i + 1] == '-')) {
            j = i + 2;
            while (j < len && s[j] >= '0' && s[j] <= '9')
                j++;
            if (j - (i + 2) == 3 && s[i + 2] == '0') {
                for (j = i + 2; j < len; j++)
                    s[j] = s[j + 1];
                len--;
            }
            break;
        }
    }
    return len;
}

static int is_nonfinite(double d)
{
    union { double d; uint64_t u; } x;
    x.d = d;
    return ((x.u >> 52) & 0x7ff) == 0x7ff;
}

int __tcc_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap0)
{
    out_t o;
    va_list ap;
    const char *p = fmt, *q;

    o.buf = buf, o.n = n, o.len = 0;
    va_copy(ap, ap0);
    while (*p) {
        int left = 0, plus = 0, space = 0, alt = 0, zero = 0;
        int width = -1, prec = -1, lm = L_NONE, c, k;
        char spec[64], sbuf[512], *s;
        int sl, slen;

        if (*p != '%') {
            for (q = p; *q && *q != '%'; q++)
                ;
            put(&o, p, q - p);
            p = q;
            continue;
        }
        q = p++;
        /* flags */
        for (;; p++) {
            if (*p == '-') left = 1;
            else if (*p == '+') plus = 1;
            else if (*p == ' ') space = 1;
            else if (*p == '#') alt = 1;
            else if (*p == '0') zero = 1;
            else break;
        }
        /* width */
        if (*p == '*') {
            width = va_arg(ap, int), p++;
            if (width < 0)
                left = 1, width = width == INT32_MIN ? INT32_MAX : -width;
        } else if (*p >= '0' && *p <= '9') {
            for (width = 0; *p >= '0' && *p <= '9'; p++)
                width = width * 10 + (*p - '0');
        }
        /* precision */
        if (*p == '.') {
            p++;
            if (*p == '*') {
                prec = va_arg(ap, int), p++;
                if (prec < 0)
                    prec = -1;
            } else {
                for (prec = 0; *p >= '0' && *p <= '9'; p++)
                    prec = prec * 10 + (*p - '0');
            }
        }
        /* length */
        switch (*p) {
        case 'h': lm = p[1] == 'h' ? (p += 2, L_HH) : (p++, L_H); break;
        case 'l': lm = p[1] == 'l' ? (p += 2, L_LL) : (p++, L_L); break;
        case 'j': lm = L_J, p++; break;
        case 'z': lm = L_Z, p++; break;
        case 't': lm = L_T, p++; break;
        case 'L': lm = L_BIGL, p++; break;
        case 'q': lm = L_LL, p++; break;
        case 'I':
            if (p[1] == '6' && p[2] == '4') lm = L_LL, p += 3;
            else if (p[1] == '3' && p[2] == '2') lm = L_NONE, p += 3;
            else lm = sizeof(void *) == 8 ? L_LL : L_NONE, p++;
            break;
        case 'w': {
            const char *r = p + 1;
            int bits = 0;
            if (*r == 'f')
                r++;
            while (*r >= '0' && *r <= '9')
                bits = bits * 10 + (*r++ - '0');
            if (r > p + 1 && r[-1] != 'f') {
                /* int_fastN_t has N bits here (win32/include/stdint.h) */
                lm = bits == 8 ? L_W8 : bits == 16 ? L_W16 : bits == 32 ? L_W32
                   : bits == 64 ? L_W64 : -1;
                if (lm == L_W32)
                    lm = L_NONE;
                p = r;
            }
            break;
        }
        }
        c = (unsigned char)*p;
        if (!c || lm < 0) { /* incomplete or invalid: copy as is */
            put(&o, q, strlen(q));
            break;
        }
        p++;

        if (c == '%') {
            put(&o, "%", 1);
            continue;
        }
        if (c == 'n') {
            store_n(&ap, lm, o.len);
            continue;
        }
        if (c == 'b' || c == 'B') {
            uint64_t v = get_uint(&ap, lm);
            char d[64], t[64 + 2 + 64];
            int nd = 0, len = 0, zpos = -1;
            while (v)
                d[nd++] = (char)('0' + (v & 1)), v >>= 1;
            if (zero && prec < 0)
                zpos = alt && nd ? 2 : 0;
            if (prec < 0)
                prec = 1;
            if (alt && nd)
                t[len++] = '0', t[len++] = (char)c;
            k = prec > nd ? prec - nd : 0;
            if (k > 64) {
                /* long run of zeros: emit directly */
                int w = width - (len + k + nd);
                if (!left) pad(&o, ' ', w);
                put(&o, t, len), pad(&o, '0', k);
                while (nd) { char ch = d[--nd]; put(&o, &ch, 1); }
                if (left) pad(&o, ' ', w);
                continue;
            }
            while (k--)
                t[len++] = '0';
            while (nd)
                t[len++] = d[--nd];
            put_padded(&o, t, len, width, left, zpos);
            continue;
        }

        /* delegate one specification to msvcrt */
        sl = 0;
        spec[sl++] = '%';
        if (left) spec[sl++] = '-';
        if (plus) spec[sl++] = '+';
        if (space) spec[sl++] = ' ';
        if (alt) spec[sl++] = '#';
        if (zero) spec[sl++] = '0';

        switch (c) {
        case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
        case 'a': case 'A': {
            double d = lm == L_BIGL ? (double)va_arg(ap, long double)
                                    : va_arg(ap, double);
            int up = c == 'E' || c == 'F' || c == 'G' || c == 'A';
            if (is_nonfinite(d)) {
                union { double d; uint64_t u; } x;
                char t[8];
                int len = 0;
                x.d = d;
                if (x.u >> 63) t[len++] = '-';
                else if (plus) t[len++] = '+';
                else if (space) t[len++] = ' ';
                memcpy(t + len, (x.u << 12) ? (up ? "NAN" : "nan")
                                             : (up ? "INF" : "inf"), 3);
                put_padded(&o, t, len + 3, width, left, -1);
                continue;
            }
            /* no width: padding is done after fixing the exponent */
            sl -= zero;
            if (prec >= 0) {
                spec[sl++] = '.';
                sl += put_num(spec + sl, prec);
            }
            spec[sl++] = c == 'F' ? 'f' : (char)c;
            spec[sl] = 0;
            s = sbuf;
            slen = ms_snprintf(sbuf, sizeof sbuf, spec, d);
            if (slen < 0 || slen >= (int)sizeof sbuf) {
                slen = ms_scprintf(spec, d);
                s = malloc(slen + 1);
                if (!s) { va_end(ap); return -1; }
                ms_snprintf(s, slen + 1, spec, d);
            }
            if (c != 'a' && c != 'A')
                slen = fix_exp(s, slen);
            k = -1;
            if (zero && !left) {
                k = (s[0] == '-' || s[0] == '+' || s[0] == ' ');
                if ((c == 'a' || c == 'A') && s[k] == '0' && (s[k + 1] | 32) == 'x')
                    k += 2;
            }
            put_padded(&o, s, slen, width, left, k);
            if (s != sbuf)
                free(s);
            continue;
        }
        }

        if (width >= 0)
            sl += put_num(spec + sl, width);
        if (prec >= 0) {
            spec[sl++] = '.';
            sl += put_num(spec + sl, prec);
        }
        s = sbuf;
        switch (c) {
        case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': {
            long long v = (c == 'd' || c == 'i') ? (long long)get_int(&ap, lm)
                                                 : (long long)get_uint(&ap, lm);
            memcpy(spec + sl, LL_, sizeof LL_ - 1), sl += sizeof LL_ - 1;
            spec[sl++] = (char)c, spec[sl] = 0;
            slen = ms_snprintf(sbuf, sizeof sbuf, spec, v);
            if (slen < 0 || slen >= (int)sizeof sbuf) {
                slen = ms_scprintf(spec, v);
                s = malloc(slen + 1);
                if (!s) { va_end(ap); return -1; }
                ms_snprintf(s, slen + 1, spec, v);
            }
            break;
        }
        case 'c': case 's': case 'p': case 'C': case 'S': {
            /* %c takes an int (or wint_t, promoted to int), the rest a pointer */
            int iv = 0;
            void *pv = 0;
            if (lm == L_L) spec[sl++] = 'l';
            else if (lm == L_H) spec[sl++] = 'h';
            spec[sl++] = (char)c, spec[sl] = 0;
            if (c == 'c' || c == 'C')
                iv = va_arg(ap, int);
            else
                pv = va_arg(ap, void *);
#define CALL(b, sz) ((c == 'c' || c == 'C') ? ms_snprintf(b, sz, spec, iv) \
                                            : ms_snprintf(b, sz, spec, pv))
            slen = CALL(sbuf, sizeof sbuf);
            if (slen < 0 || slen >= (int)sizeof sbuf) {
                slen = (c == 'c' || c == 'C') ? ms_scprintf(spec, iv) : ms_scprintf(spec, pv);
                s = malloc(slen + 1);
                if (!s) { va_end(ap); return -1; }
                CALL(s, slen + 1);
            }
#undef CALL
            break;
        }
        default: /* unknown conversion: copy as is */
            put(&o, q, p - q);
            continue;
        }
        if (slen > 0)
            put(&o, s, slen);
        if (s != sbuf)
            free(s);
    }
    va_end(ap);
    if (n)
        o.buf[o.len < n ? o.len : n - 1] = 0;
    return o.len > 0x7fffffff ? -1 : (int)o.len;
}

int __tcc_snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = __tcc_vsnprintf(buf, n, fmt, ap); va_end(ap);
    return r;
}

int __tcc_vsprintf(char *buf, const char *fmt, va_list ap)
{
    return __tcc_vsnprintf(buf, (size_t)0x7fffffff, fmt, ap);
}

int __tcc_sprintf(char *buf, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = __tcc_vsprintf(buf, fmt, ap); va_end(ap);
    return r;
}

int __tcc_vfprintf(FILE_ *fp, const char *fmt, va_list ap)
{
    char sbuf[1024], *s = sbuf;
    va_list ap2;
    int r;
    va_copy(ap2, ap);
    r = __tcc_vsnprintf(sbuf, sizeof sbuf, fmt, ap);
    if (r >= (int)sizeof sbuf) {
        s = malloc(r + 1);
        if (s)
            __tcc_vsnprintf(s, r + 1, fmt, ap2);
        else
            r = -1;
    }
    va_end(ap2);
    if (r > 0 && fwrite(s, 1, r, fp) != (size_t)r)
        r = -1;
    if (s != sbuf)
        free(s);
    return r;
}

int __tcc_fprintf(FILE_ *fp, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = __tcc_vfprintf(fp, fmt, ap); va_end(ap);
    return r;
}

int __tcc_vprintf(const char *fmt, va_list ap)
{
    return __tcc_vfprintf(STDOUT_, fmt, ap);
}

int __tcc_printf(const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = __tcc_vfprintf(STDOUT_, fmt, ap); va_end(ap);
    return r;
}
