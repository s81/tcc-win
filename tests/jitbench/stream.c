/*
 * JIT-compiled stream loops over a file larger than 4 GiB (docs/JIT-BASELINE.md).
 * The file is written by "jitbench mkfile": byte at offset o is pat(o).
 * Each function: int fn(const char *path, uint64_t out[8]); returns 0 if consistent.
 *   out[0] bytes processed   out[1] byte sum   out[2] pattern mismatches
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>

static uint8_t pat(uint64_t o) { return (uint8_t)(o ^ (o >> 13) ^ ((o >> 32) * 7)); }

#define PROBE ((4ull << 30) + 12345) /* an offset that does not fit in 32 bits */

/* C stdio: fread in 1 MiB chunks, 64-bit counters */
int stream_stdio(const char *path, uint64_t *out)
{
    enum { BS = 1 << 20 };
    FILE *f = fopen(path, "rb");
    uint8_t *buf = malloc(BS);
    uint64_t total = 0, sum = 0, bad = 0;
    size_t n;
    fpos_t pos;
    if (!f || !buf)
        return -1;
    while ((n = fread(buf, 1, BS, f)) > 0) {
        for (size_t i = 0; i < n; i++)
            sum += buf[i];
        bad += buf[0] != pat(total) || buf[n - 1] != pat(total + n - 1);
        total += n;
    }
    out[0] = total, out[1] = sum, out[2] = bad;
    out[3] = (uint64_t)(int64_t)ftell(f);          /* long is 32-bit (LLP64) */
    fgetpos(f, &pos), out[4] = (uint64_t)pos;      /* fpos_t is 64-bit */
    /* 64-bit seek, then a 32-bit seek to the same offset for comparison */
    out[5] = _fseeki64(f, PROBE, SEEK_SET) == 0 && fgetc(f) == pat(PROBE);
    out[6] = fseek(f, (long)PROBE, SEEK_SET) == 0 && fgetc(f) == pat(PROBE);
    out[7] = sizeof(size_t) * 100 + sizeof(long) * 10 + sizeof(fpos_t);
    fclose(f), free(buf);
    return bad || !out[5];
}

/* Win32: ReadFile in 1 MiB chunks, SetFilePointerEx */
int stream_win32(const char *path, uint64_t *out)
{
    enum { BS = 1 << 20 };
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    uint8_t *buf = malloc(BS), c = 0;
    uint64_t total = 0, sum = 0, bad = 0;
    LARGE_INTEGER li, np;
    DWORD n, hi = 0, lo;
    if (h == INVALID_HANDLE_VALUE || !buf)
        return -1;
    while (ReadFile(h, buf, BS, &n, NULL) && n) {
        for (DWORD i = 0; i < n; i++)
            sum += buf[i];
        bad += buf[0] != pat(total) || buf[n - 1] != pat(total + n - 1);
        total += n;
    }
    out[0] = total, out[1] = sum, out[2] = bad;
    lo = GetFileSize(h, &hi);
    out[3] = ((uint64_t)hi << 32) | lo;
    li.QuadPart = PROBE;
    out[5] = SetFilePointerEx(h, li, &np, FILE_BEGIN) && np.QuadPart == (LONGLONG)PROBE
        && ReadFile(h, &c, 1, &n, NULL) && c == pat(PROBE);
    CloseHandle(h), free(buf);
    return bad || !out[5] || out[3] != total;
}

/* one flat mapping of the whole file, indexed with a 64-bit size_t */
int stream_mmap(const char *path, uint64_t *out)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE m;
    const uint8_t *p;
    uint64_t sum = 0, bad = 0;
    DWORD hi = 0, lo;
    size_t size;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    lo = GetFileSize(h, &hi);
    size = ((size_t)hi << 32) | lo;
    m = CreateFileMappingA(h, NULL, PAGE_READONLY, 0, 0, NULL);
    p = m ? MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0) : NULL;
    if (!p)
        return -2;
    for (size_t i = 0; i < size; i++)
        sum += p[i];
    for (size_t i = 0; i < size; i += 999983)
        bad += p[i] != pat(i);
    out[0] = size, out[1] = sum, out[2] = bad;
    out[5] = p[PROBE] == pat(PROBE);
    UnmapViewOfFile(p), CloseHandle(m), CloseHandle(h);
    return bad || !out[5];
}
