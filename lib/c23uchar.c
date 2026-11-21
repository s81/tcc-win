/* <uchar.h> conversions for Windows targets (part of libtcc1.a):
   mbrtoc8, c8rtomb, mbrtoc16, c16rtomb, mbrtoc32, c32rtomb.

   The multibyte side is the C runtime's: they call msvcrt's mbrtowc and
   wcrtomb, so the multibyte encoding is that of the current locale
   (msvcrt.dll has no UTF-8 locale).  msvcrt's wchar_t is a UTF-16 code
   unit, so a multibyte character that does not fit a single UTF-16 code
   unit (outside the BMP) is not supported: such conversions fail with
   EILSEQ.  Each function keeps its own state in the 32 bits of mbstate_t:

     bits 28..31  0: initial; 1..3: that many incomplete input bytes in
                  bits 0..23; 4: a low surrogate in bits 0..15 (c16);
                  5..7: 1..3 UTF-8 continuation bytes still to be
                  delivered, in bits 0..23 (c8)

   Without _WIN32 this builds against the host C library for tests (rename
   the public names with -D, see tests/c99rt-host.sh). */

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef _WIN32
__declspec(dllimport) int *_errno(void);
#define SET_EILSEQ() (*_errno() = 42)
#else
#include <errno.h>
#define SET_EILSEQ() (errno = EILSEQ)
#endif

typedef unsigned short c16_t;
typedef unsigned int c32_t;
typedef unsigned char c8_t;

#define TAG(st) ((st) >> 28)
#define DATA(st) ((st) & 0x0fffffffu)

static unsigned int internal_state;

static unsigned int *state(mbstate_t *ps)
{
    return ps ? (unsigned int *)ps : &internal_state;
}

/* Reads one multibyte character from s (n bytes), with the incomplete bytes
   of an earlier call in *st.  Returns the number of bytes of s used, 0 for
   the null character, (size_t)-1 on an error, (size_t)-2 for an incomplete
   character (all n bytes saved).  *wc is the UTF-16 code unit. */
static size_t mb_read(unsigned int *st, const char *s, size_t n, unsigned *wc)
{
    char buf[4];
    int have = TAG(*st) >= 1 && TAG(*st) <= 3 ? (int)TAG(*st) : 0, cnt, i;
    size_t used = 0;
    unsigned int d = DATA(*st);

    for (i = 0; i < have; i++)
        buf[i] = (char)(d >> (8 * i));
    cnt = have;
    for (;;) {
        wchar_t w;
        size_t r;
        int zero = 0;
        if (used == n) {
            if (cnt > 3)
                break;
            *st = (unsigned)cnt << 28;
            for (i = 0; i < cnt; i++)
                *st |= (unsigned)(unsigned char)buf[i] << (8 * i);
            return (size_t)-2;
        }
        if (cnt == 4)
            break;
        buf[cnt++] = s[used++];
        r = mbrtowc(&w, buf, cnt, &zero);
        if (r == (size_t)-2)
            continue;
        if (r == (size_t)-1)
            break;
        *st = 0;
        *wc = (unsigned short)w;
        return r == 0 ? 0 : used;
    }
    *st = 0;
    SET_EILSEQ();
    return (size_t)-1;
}

static size_t wc_write(char *s, unsigned wc)
{
    char tmp[8];
    int zero = 0;
    size_t r = wcrtomb(tmp, (wchar_t)wc, &zero);
    size_t i;
    if (r == (size_t)-1 || r > 8) {
        SET_EILSEQ();
        return (size_t)-1;
    }
    for (i = 0; i < r; i++)
        s[i] = tmp[i];
    return r;
}

size_t mbrtoc16(c16_t *pc, const char *s, size_t n, mbstate_t *ps)
{
    unsigned int *st = state(ps);
    unsigned wc;
    size_t r;
    if (!s) {
        *st = 0;
        return 0;
    }
    if (TAG(*st) == 4) { /* low surrogate of the previous character */
        if (pc)
            *pc = (c16_t)DATA(*st);
        *st = 0;
        return (size_t)-3;
    }
    r = mb_read(st, s, n, &wc);
    if (r == (size_t)-1 || r == (size_t)-2)
        return r;
    if (pc)
        *pc = (c16_t)wc;
    return r;
}

size_t mbrtoc32(c32_t *pc, const char *s, size_t n, mbstate_t *ps)
{
    unsigned int *st = state(ps);
    unsigned wc;
    size_t r;
    if (!s) {
        *st = 0;
        return 0;
    }
    r = mb_read(st, s, n, &wc);
    if (r == (size_t)-1 || r == (size_t)-2)
        return r;
    if (pc)
        *pc = wc;
    return r;
}

size_t mbrtoc8(c8_t *pc, const char *s, size_t n, mbstate_t *ps)
{
    unsigned int *st = state(ps);
    unsigned wc;
    size_t r;
    if (!s) {
        *st = 0;
        return 0;
    }
    if (TAG(*st) >= 5) { /* continuation bytes of the previous character */
        unsigned int t = TAG(*st), d = DATA(*st);
        if (pc)
            *pc = (c8_t)d;
        d >>= 8;
        *st = t > 5 ? ((t - 1) << 28) | d : 0;
        return (size_t)-3;
    }
    r = mb_read(st, s, n, &wc);
    if (r == (size_t)-1 || r == (size_t)-2)
        return r;
    if (wc >= 0xD800 && wc <= 0xDFFF) { /* a lone surrogate */
        *st = 0;
        SET_EILSEQ();
        return (size_t)-1;
    }
    if (wc < 0x80) {
        if (pc)
            *pc = (c8_t)wc;
    } else if (wc < 0x800) {
        if (pc)
            *pc = (c8_t)(0xC0 | wc >> 6);
        *st = (5u << 28) | (0x80 | (wc & 0x3f));
    } else {
        if (pc)
            *pc = (c8_t)(0xE0 | wc >> 12);
        *st = (6u << 28) | (0x80 | (wc >> 6 & 0x3f)) | (0x80 | (wc & 0x3f)) << 8;
    }
    return r == 0 && wc == 0 ? 0 : r;
}

size_t c16rtomb(char *s, c16_t c, mbstate_t *ps)
{
    unsigned int *st = state(ps);
    if (!s) {
        *st = 0;
        return 1;
    }
    if (TAG(*st) == 4) { /* c must be the low surrogate */
        *st = 0;
        SET_EILSEQ(); /* the pair is outside the BMP: see the file comment */
        return (size_t)-1;
    }
    if (c >= 0xD800 && c <= 0xDBFF) {
        *st = (4u << 28) | c;
        return 0;
    }
    if (c >= 0xDC00 && c <= 0xDFFF) {
        SET_EILSEQ();
        return (size_t)-1;
    }
    return wc_write(s, c);
}

size_t c32rtomb(char *s, c32_t c, mbstate_t *ps)
{
    unsigned int *st = state(ps);
    if (!s) {
        *st = 0;
        return 1;
    }
    if (c > 0xFFFF || (c >= 0xD800 && c <= 0xDFFF)) {
        SET_EILSEQ();
        return (size_t)-1;
    }
    return wc_write(s, c);
}

size_t c8rtomb(char *s, c8_t c, mbstate_t *ps)
{
    unsigned int *st = state(ps);
    unsigned int t = TAG(*st), d = DATA(*st);
    unsigned wc;
    /* the state holds the UTF-8 bytes of an incomplete character in bits 0..23
       (tags 1..3 = number of bytes); decode when complete */
    if (!s) {
        *st = 0;
        return 1;
    }
    if (t == 0) {
        if (c < 0x80)
            return wc_write(s, c);
        if (c < 0xC2 || c > 0xEF) {
            SET_EILSEQ();
            return (size_t)-1;
        }
        *st = (1u << 28) | c;
        return 0;
    }
    if ((c & 0xC0) != 0x80) {
        *st = 0;
        SET_EILSEQ();
        return (size_t)-1;
    }
    d |= (unsigned)c << (8 * t);
    t++;
    {
        unsigned lead = d & 0xff;
        int need = lead >= 0xE0 ? 3 : 2;
        if ((int)t < need) {
            *st = (t << 28) | d;
            return 0;
        }
        *st = 0;
        if (need == 2)
            wc = (lead & 0x1f) << 6 | (d >> 8 & 0x3f);
        else
            wc = (lead & 0x0f) << 12 | (d >> 8 & 0x3f) << 6 | (d >> 16 & 0x3f);
        if ((need == 2 && wc < 0x80) || (need == 3 && wc < 0x800)
            || (wc >= 0xD800 && wc <= 0xDFFF)) {
            SET_EILSEQ();
            return (size_t)-1;
        }
    }
    return wc_write(s, wc);
}
