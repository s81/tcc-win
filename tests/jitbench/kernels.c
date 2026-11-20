/*
 * Stream kernels for the JIT baseline (docs/JIT-BASELINE.md).
 * All have the signature  uint64_t k_NAME(const uint8_t *src, uint8_t *dst, size_t n).
 * Define K_NAME to compile a single kernel as its own unit, nothing for all.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined K_bytesum && !defined K_xor && !defined K_upper && !defined K_wc \
 && !defined K_fnv1a && !defined K_crc32 && !defined K_sum32 && !defined K_hist \
 && !defined K_delta32 && !defined K_copy64 && !defined K_dsum && !defined K_daxpy
# define K_ALL 1
#endif

#if K_ALL || K_bytesum /* reduction over bytes */
uint64_t k_bytesum(const uint8_t *src, uint8_t *dst, size_t n)
{
    uint64_t a = 0;
    for (size_t i = 0; i < n; i++)
        a += src[i];
    return a;
}
#endif

#if K_ALL || K_xor /* read / transform / write */
uint64_t k_xor(const uint8_t *src, uint8_t *dst, size_t n)
{
    for (size_t i = 0; i < n; i++)
        dst[i] = src[i] ^ 0x5a;
    return n;
}
#endif

#if K_ALL || K_upper /* branchy byte transform */
uint64_t k_upper(const uint8_t *src, uint8_t *dst, size_t n)
{
    uint64_t changed = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = src[i];
        if (c >= 'a' && c <= 'z')
            c -= 32, changed++;
        dst[i] = c;
    }
    return changed;
}
#endif

#if K_ALL || K_wc /* byte state machine: words and lines */
uint64_t k_wc(const uint8_t *src, uint8_t *dst, size_t n)
{
    uint64_t words = 0, lines = 0;
    int in = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = src[i];
        if (c == '\n')
            lines++;
        if (c == ' ' || c == '\n' || c == '\t')
            in = 0;
        else if (!in)
            in = 1, words++;
    }
    return (words << 32) ^ lines;
}
#endif

#if K_ALL || K_fnv1a /* serial dependency chain */
uint64_t k_fnv1a(const uint8_t *src, uint8_t *dst, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++)
        h = (h ^ src[i]) * 0x100000001b3ull;
    return h;
}
#endif

#if K_ALL || K_crc32 /* table lookup */
uint64_t k_crc32(const uint8_t *src, uint8_t *dst, size_t n)
{
    static uint32_t tab[256];
    uint32_t c = 0xffffffffu;
    if (!tab[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t r = i;
            for (int k = 0; k < 8; k++)
                r = r & 1 ? 0xedb88320u ^ (r >> 1) : r >> 1;
            tab[i] = r;
        }
    for (size_t i = 0; i < n; i++)
        c = tab[(c ^ src[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}
#endif

#if K_ALL || K_sum32 /* reduction over 32-bit words */
uint64_t k_sum32(const uint8_t *src, uint8_t *dst, size_t n)
{
    const uint32_t *p = (const uint32_t *)src;
    uint64_t a = 0;
    for (size_t i = 0; i < n / 4; i++)
        a += p[i];
    return a;
}
#endif

#if K_ALL || K_hist /* byte histogram */
uint64_t k_hist(const uint8_t *src, uint8_t *dst, size_t n)
{
    uint64_t h[256], x = 0;
    memset(h, 0, sizeof h);
    for (size_t i = 0; i < n; i++)
        h[src[i]]++;
    for (int i = 0; i < 256; i++)
        x = x * 1000003u + h[i];
    return x;
}
#endif

#if K_ALL || K_delta32 /* word transform with a carried value */
uint64_t k_delta32(const uint8_t *src, uint8_t *dst, size_t n)
{
    const uint32_t *p = (const uint32_t *)src;
    uint32_t *q = (uint32_t *)dst, prev = 0;
    for (size_t i = 0; i < n / 4; i++) {
        uint32_t v = p[i];
        q[i] = v - prev;
        prev = v;
    }
    return prev;
}
#endif

#if K_ALL || K_copy64 /* plain 64-bit word copy */
uint64_t k_copy64(const uint8_t *src, uint8_t *dst, size_t n)
{
    const uint64_t *p = (const uint64_t *)src;
    uint64_t *q = (uint64_t *)dst;
    for (size_t i = 0; i < n / 8; i++)
        q[i] = p[i];
    return n / 8;
}
#endif

#if K_ALL || K_dsum /* binary64 reduction (order must be preserved) */
uint64_t k_dsum(const uint8_t *src, uint8_t *dst, size_t n)
{
    const double *p = (const double *)src;
    double a = 0.0;
    uint64_t bits;
    for (size_t i = 0; i < n / 8; i++)
        a += p[i];
    memcpy(&bits, &a, 8);
    return bits;
}
#endif

#if K_ALL || K_daxpy /* binary64 transform (must not be contracted to FMA) */
uint64_t k_daxpy(const uint8_t *src, uint8_t *dst, size_t n)
{
    const double *x = (const double *)src;
    double *y = (double *)dst, a = 1.0000001;
    uint64_t bits;
    for (size_t i = 0; i < n / 8; i++)
        y[i] = a * x[i] + y[i];
    memcpy(&bits, &y[n / 8 - 1], 8);
    return bits;
}
#endif
