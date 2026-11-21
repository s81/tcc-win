/* The scanf family for Windows targets (part of libtcc1.a).

   msvcrt.dll has no vsscanf, vfscanf, vscanf (or wide versions), treats %hhd
   as %hd and has no %b.  This is a C99/C23 scanf: the conversions
   d i u o x X b B c s [ p n a A e E f F g G %, the flags/modifiers *, width,
   hh h l ll j z t L (and I64/I32/I for msvcrt compatibility).  %b reads a
   binary number (0b/0B prefix allowed), %i accepts 0b like strtol does in
   C23.  Wide variants follow the msvcrt convention: an unqualified %s, %c
   or %[ in the wide functions stores wchar_t (use %hs for char), and %ls in
   the narrow ones converts with mbrtowc.

   In C23 mode <stdio.h>/<wchar.h> map sscanf, fscanf, scanf, swscanf,
   fwscanf and wscanf to the __tcc_ versions here (opt out with
   -D__TCC_MSVCRT_SCANF).  Not supported: the 'n$' form and multibyte
   state across calls.  After a "0x" or "0b" prefix a digit must follow,
   else the conversion fails (the prefix stays consumed: one character of
   push-back is all a stream guarantees).

   Without _WIN32 this builds against the host C library for tests
   (rename the public names with -D, see tests/c99rt-host.sh). */

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef _WIN32
typedef struct _iobuf FILE_;
__declspec(dllimport) int fgetc(FILE_ *);
__declspec(dllimport) int ungetc(int, FILE_ *);
__declspec(dllimport) unsigned short fgetwc(FILE_ *);
__declspec(dllimport) unsigned short ungetwc(unsigned short, FILE_ *);
__declspec(dllimport) double strtod(const char *, char **);
__declspec(dllimport) size_t mbrtowc(wchar_t *, const char *, size_t, void *);
__declspec(dllimport) size_t wcrtomb(char *, wchar_t, void *);
__declspec(dllimport) int isspace(int);
__declspec(dllimport) int iswspace(unsigned short);
__declspec(dllimport) char *__iob_func(void);
#define STDIN_ ((FILE_ *)__iob_func())
#define WEOF_ 0xffff
#else
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
typedef FILE FILE_;
#define STDIN_ stdin
#define WEOF_ WEOF
#endif

typedef struct {
    int wide;         /* input is wchar_t */
    const void *sp;   /* string input */
    FILE_ *fp;        /* or file input */
    int nread;        /* characters consumed */
    int ungot;        /* last character is pushed back (end of string input) */
} src_t;

static int sget(src_t *s)
{
    int c;
    if (!s->fp) {
        if (s->wide) {
            c = *(const wchar_t *)s->sp;
            if (!c)
                return -1;
            s->sp = (const wchar_t *)s->sp + 1;
        } else {
            c = *(const unsigned char *)s->sp;
            if (!c)
                return -1;
            s->sp = (const char *)s->sp + 1;
        }
    } else if (s->wide) {
        c = fgetwc(s->fp);
        if (c == WEOF_)
            return -1;
    } else {
        c = fgetc(s->fp);
        if (c < 0)
            return -1;
    }
    s->nread++;
    return c;
}

static void sunget(src_t *s, int c)
{
    if (c < 0)
        return;
    s->nread--;
    if (!s->fp) {
        if (s->wide)
            s->sp = (const wchar_t *)s->sp - 1;
        else
            s->sp = (const char *)s->sp - 1;
    } else if (s->wide) {
        ungetwc((unsigned short)c, s->fp);
    } else {
        ungetc(c, s->fp);
    }
}

static int is_space(const src_t *s, int c)
{
    return s->wide ? iswspace((unsigned short)c) : (c >= 0 && c < 256 && isspace(c));
}

static int digit_of(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 99;
}

enum { L_NONE, L_HH, L_H, L_L, L_LL, L_J, L_Z, L_T, L_BIGL };

#define FC(i) (wfmt ? (int)((const wchar_t *)fmt)[i] : (int)((const unsigned char *)fmt)[i])

/* is c in the scanset fmt[i..j) (after '[' and '^')? */
static int in_set(const void *fmt, int wfmt, int i, int j, int c)
{
    int k;
    for (k = i; k < j; k++) {
        int lo = FC(k);
        if (k + 2 < j && FC(k + 1) == '-') {
            int hi = FC(k + 2);
            if (c >= lo && c <= hi)
                return 1;
            k += 2;
        } else if (c == lo) {
            return 1;
        }
    }
    return 0;
}

static void store_int(va_list *ap, int lm, unsigned long long v)
{
    switch (lm) {
    case L_HH: *va_arg(*ap, unsigned char *) = (unsigned char)v; break;
    case L_H: *va_arg(*ap, unsigned short *) = (unsigned short)v; break;
    case L_L: *va_arg(*ap, unsigned long *) = (unsigned long)v; break;
    case L_LL: *va_arg(*ap, unsigned long long *) = v; break;
    case L_J: *va_arg(*ap, uintmax_t *) = v; break;
    case L_Z: *va_arg(*ap, size_t *) = (size_t)v; break;
    case L_T: *va_arg(*ap, ptrdiff_t *) = (ptrdiff_t)v; break;
    default: *va_arg(*ap, unsigned int *) = (unsigned int)v; break;
    }
}

/* where %c, %s and %[ put their characters */
typedef struct {
    void *p;          /* NULL when assignment is suppressed */
    int dwide;        /* destination is wchar_t */
    int swide;        /* source is wchar_t */
    int n;            /* elements stored */
    int mb;           /* multibyte conversion state: bytes pending */
    char mbuf[8];
} dst_t;

/* returns 0 on success, -1 on a conversion error */
static int dput(dst_t *d, int c)
{
    if (d->dwide == d->swide) {
        if (d->p) {
            if (d->dwide)
                ((wchar_t *)d->p)[d->n] = (wchar_t)c;
            else
                ((char *)d->p)[d->n] = (char)c;
        }
        d->n++;
    } else if (d->dwide) { /* narrow input, wide destination: mbrtowc */
        wchar_t w;
        size_t r;
        d->mbuf[d->mb++] = (char)c;
        r = mbrtowc(&w, d->mbuf, d->mb, 0);
        if (r == (size_t)-2 && d->mb < 8)
            return 0;
        if (r == (size_t)-1 || r == (size_t)-2)
            return -1;
        d->mb = 0;
        if (d->p)
            ((wchar_t *)d->p)[d->n] = w;
        d->n++;
    } else { /* wide input, narrow destination: wcrtomb */
        char b[8];
        size_t r = wcrtomb(b, (wchar_t)c, 0), i;
        if (r == (size_t)-1)
            return -1;
        for (i = 0; i < r; i++) {
            if (d->p)
                ((char *)d->p)[d->n] = b[i];
            d->n++;
        }
    }
    return 0;
}

static void dterm(dst_t *d)
{
    if (!d->p)
        return;
    if (d->dwide)
        ((wchar_t *)d->p)[d->n] = 0;
    else
        ((char *)d->p)[d->n] = 0;
}

static int scan(src_t *s, const void *fmt, int wfmt, va_list ap0)
{
    va_list ap;
    int i = 0, assigned = 0, c, failed_input = 0;

    va_copy(ap, ap0);
    for (;;) {
        int f = FC(i), suppress = 0, width = -1, lm = L_NONE, base, neg, w, nd;
        int specstart;

        if (!f)
            break;
        if (is_space(s, f) && f < 128) {
            /* white space in the format: skip all white space in the input */
            do c = sget(s); while (c >= 0 && is_space(s, c));
            sunget(s, c);
            i++;
            continue;
        }
        if (f != '%') {
            c = sget(s);
            if (c < 0) { failed_input = 1; break; }
            if (c != f) { sunget(s, c); break; }
            i++;
            continue;
        }
        i++;
        if (FC(i) == '%') {
            do c = sget(s); while (c >= 0 && is_space(s, c));
            if (c < 0) { failed_input = 1; break; }
            if (c != '%') { sunget(s, c); break; }
            i++;
            continue;
        }
        if (FC(i) == '*')
            suppress = 1, i++;
        if (FC(i) >= '0' && FC(i) <= '9') {
            for (width = 0; FC(i) >= '0' && FC(i) <= '9'; i++)
                width = width * 10 + (FC(i) - '0');
            if (width == 0)
                break;
        }
        switch (FC(i)) {
        case 'h': if (FC(i + 1) == 'h') lm = L_HH, i += 2; else lm = L_H, i++; break;
        case 'l': if (FC(i + 1) == 'l') lm = L_LL, i += 2; else lm = L_L, i++; break;
        case 'j': lm = L_J, i++; break;
        case 'z': lm = L_Z, i++; break;
        case 't': lm = L_T, i++; break;
        case 'L': case 'q': lm = L_BIGL, i++; break;
        case 'I':
            if (FC(i + 1) == '6' && FC(i + 2) == '4') lm = L_LL, i += 3;
            else if (FC(i + 1) == '3' && FC(i + 2) == '2') lm = L_NONE, i += 3;
            else lm = sizeof(void *) == 8 ? L_LL : L_NONE, i++;
            break;
        }
        specstart = i;
        f = FC(i++);
        if (f == 'n') {
            if (!suppress)
                store_int(&ap, lm, (unsigned long long)s->nread);
            continue;
        }
        if (f == 'C') f = 'c', lm = L_L;
        if (f == 'S') f = 's', lm = L_L;

        if (f == 'c' || f == 's' || f == '[') {
            dst_t d;
            int setneg = 0, sa = 0, sb = 0;
            d.dwide = s->wide ? lm != L_H : lm == L_L;
            d.swide = s->wide;
            d.p = suppress ? NULL : va_arg(ap, void *);
            d.n = d.mb = 0;
            if (f == '[') {
                if (FC(i) == '^')
                    setneg = 1, i++;
                sa = i;
                if (FC(i) == ']')
                    i++;
                while (FC(i) && FC(i) != ']')
                    i++;
                sb = i;
                if (FC(i) == ']')
                    i++;
            }
            if (f != 'c' && f != '[') {
                do c = sget(s); while (c >= 0 && is_space(s, c));
                sunget(s, c);
            }
            w = f == 'c' && width < 0 ? 1 : width;
            nd = 0;
            for (;;) {
                if (w >= 0 && nd >= w)
                    break;
                c = sget(s);
                if (c < 0)
                    break;
                if (f == 's' && is_space(s, c)) { sunget(s, c); break; }
                if (f == '[' && (in_set(fmt, wfmt, sa, sb, c) == setneg)) { sunget(s, c); break; }
                if (dput(&d, c) < 0)
                    goto done;
                nd++;
            }
            if (f == 'c' ? nd < w : nd == 0) {
                if (nd == 0 && c < 0)
                    failed_input = 1;
                if (f == 'c' && nd > 0 && c >= 0)
                    ;
                goto done;
            }
            if (f != 'c')
                dterm(&d);
            if (!suppress)
                assigned++;
            continue;
        }

        do c = sget(s); while (c >= 0 && is_space(s, c));
        if (c < 0) { failed_input = 1; goto done; }

        switch (f) {
        case 'd': base = 10; break;
        case 'u': base = 10; break;
        case 'i': base = 0; break;
        case 'o': base = 8; break;
        case 'x': case 'X': case 'p': base = 16; break;
        case 'b': case 'B': base = 2; break;
        case 'a': case 'A': case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
            base = -1; break;
        default:
            sunget(s, c);
            (void)specstart;
            goto done;
        }
        w = width < 0 ? 0x7fffffff : width;

        if (base < 0) { /* floating point */
            char b[512];
            int n = 0, seen_digit = 0, hex = 0;
            double v;
            long double lv;
            if (c == '+' || c == '-') {
                b[n++] = (char)c; w--;
                c = w > 0 ? sget(s) : -1;
            }
            if ((c | 32) == 'i' || (c | 32) == 'n') {
                /* inf, infinity, nan, nan(chars) */
                const char *word = (c | 32) == 'i' ? "infinity" : "nan";
                int k = 0, wl = (c | 32) == 'i' ? 8 : 3;
                while (c >= 0 && w > 0 && k < wl && (c | 32) == word[k]) {
                    b[n++] = (char)c, k++, w--;
                    c = w > 0 ? sget(s) : -1;
                }
                if (!((k == 3 && wl == 8) || k == wl || (k == 8))) {
                    sunget(s, c);
                    goto done;
                }
                if (wl == 3 && c == '(' && w > 0) {
                    b[n++] = '(';
                    for (;;) {
                        c = sget(s), w--;
                        if (c < 0 || n > 500) break;
                        b[n++] = (char)c;
                        if (c == ')') { c = w > 0 ? sget(s) : -1; break; }
                    }
                }
                sunget(s, c);
            } else {
                if (c == '0' && w > 0) {
                    b[n++] = '0'; w--; seen_digit = 1;
                    c = w > 0 ? sget(s) : -1;
                    if ((c | 32) == 'x' && w > 0) {
                        b[n++] = (char)c; w--; hex = 1;
                        c = w > 0 ? sget(s) : -1;
                    }
                }
                while (c >= 0 && n < 500 && digit_of(c) < (hex ? 16 : 10) && w > 0) {
                    b[n++] = (char)c, w--, seen_digit = 1;
                    c = w > 0 ? sget(s) : -1;
                }
                if (c == '.' && w > 0) {
                    b[n++] = '.'; w--;
                    c = w > 0 ? sget(s) : -1;
                    while (c >= 0 && n < 500 && digit_of(c) < (hex ? 16 : 10) && w > 0) {
                        b[n++] = (char)c, w--, seen_digit = 1;
                        c = w > 0 ? sget(s) : -1;
                    }
                }
                if (!seen_digit) {
                    sunget(s, c);
                    goto done;
                }
                if (c >= 0 && w > 0 && (hex ? (c | 32) == 'p' : (c | 32) == 'e')) {
                    int sgn = 0, ed = 0;
                    b[n++] = (char)c; w--;
                    c = w > 0 ? sget(s) : -1;
                    if ((c == '+' || c == '-') && w > 0) {
                        b[n++] = (char)c; w--, sgn = 1;
                        c = w > 0 ? sget(s) : -1;
                    }
                    while (c >= 0 && n < 500 && c >= '0' && c <= '9' && w > 0) {
                        b[n++] = (char)c, w--, ed = 1;
                        c = w > 0 ? sget(s) : -1;
                    }
                    if (!ed)
                        n -= 1 + sgn; /* "1e" or "1e+": the exponent is dropped */
                }
                sunget(s, c);
            }
            b[n] = 0;
            v = strtod(b, 0);
            lv = v;
            if (!suppress) {
                if (lm == L_BIGL)
                    *va_arg(ap, long double *) = lv;
                else if (lm == L_L)
                    *va_arg(ap, double *) = v;
                else
                    *va_arg(ap, float *) = (float)v;
                assigned++;
            }
            continue;
        }

        /* integers */
        neg = 0;
        nd = 0;
        {
            unsigned long long v = 0;
            if (c == '+' || c == '-') {
                neg = c == '-';
                w--;
                c = w > 0 ? sget(s) : -1;
            }
            if ((base == 0 || base == 16 || base == 2 || base == 8) && c == '0' && w > 0) {
                nd = 1; w--;
                c = w > 0 ? sget(s) : -1;
                if ((base == 0 || base == 16) && (c | 32) == 'x' && w > 0) {
                    base = 16; nd = 0; w--;
                    c = w > 0 ? sget(s) : -1;
                } else if ((base == 0 || base == 2) && (c | 32) == 'b' && w > 0) {
                    base = 2; nd = 0; w--;
                    c = w > 0 ? sget(s) : -1;
                } else if (base == 0) {
                    base = 8;
                }
            } else if (base == 0) {
                base = 10;
            }
            while (c >= 0 && w > 0 && digit_of(c) < base) {
                v = v * base + digit_of(c);
                nd++, w--;
                c = w > 0 ? sget(s) : -1;
            }
            if (c >= 0 && w <= 0 && 0)
                ;
            sunget(s, c);
            if (!nd) {
                if (c < 0)
                    failed_input = 1;
                goto done;
            }
            if (neg)
                v = 0 - v;
            if (!suppress) {
                if (f == 'p')
                    *va_arg(ap, void **) = (void *)(uintptr_t)v;
                else
                    store_int(&ap, lm, v);
                assigned++;
            }
        }
    }
done:
    va_end(ap);
    if (assigned == 0 && failed_input)
        return -1;
    return assigned;
}

/* ---- public functions ---- */

int vsscanf(const char *str, const char *fmt, va_list ap)
{
    src_t s;
    s.wide = 0, s.sp = str, s.fp = 0, s.nread = 0, s.ungot = 0;
    return scan(&s, fmt, 0, ap);
}

int vfscanf(FILE_ *fp, const char *fmt, va_list ap)
{
    src_t s;
    s.wide = 0, s.sp = 0, s.fp = fp, s.nread = 0, s.ungot = 0;
    return scan(&s, fmt, 0, ap);
}

int vscanf(const char *fmt, va_list ap)
{
    return vfscanf(STDIN_, fmt, ap);
}

int vswscanf(const wchar_t *str, const wchar_t *fmt, va_list ap)
{
    src_t s;
    s.wide = 1, s.sp = str, s.fp = 0, s.nread = 0, s.ungot = 0;
    return scan(&s, fmt, 1, ap);
}

int vfwscanf(FILE_ *fp, const wchar_t *fmt, va_list ap)
{
    src_t s;
    s.wide = 1, s.sp = 0, s.fp = fp, s.nread = 0, s.ungot = 0;
    return scan(&s, fmt, 1, ap);
}

int vwscanf(const wchar_t *fmt, va_list ap)
{
    return vfwscanf(STDIN_, fmt, ap);
}

#define WRAP(name, vname, first_t, first, fmt_t) \
int name(first_t first, fmt_t fmt, ...) \
{ va_list ap; int r; va_start(ap, fmt); r = vname(first, fmt, ap); va_end(ap); return r; }

WRAP(__tcc_sscanf, vsscanf, const char *, str, const char *)
WRAP(__tcc_fscanf, vfscanf, FILE_ *, fp, const char *)
WRAP(__tcc_swscanf, vswscanf, const wchar_t *, str, const wchar_t *)
WRAP(__tcc_fwscanf, vfwscanf, FILE_ *, fp, const wchar_t *)

int __tcc_scanf(const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vscanf(fmt, ap); va_end(ap);
    return r;
}

int __tcc_wscanf(const wchar_t *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vwscanf(fmt, ap); va_end(ap);
    return r;
}
