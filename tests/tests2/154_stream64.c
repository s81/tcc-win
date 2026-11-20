/* 64-bit file offsets on Windows, where long is 32 bits (LLP64).
   Seeks past 4 GiB without writing there, so the file stays 3 bytes. */
#include <stdio.h>
#include <io.h>
#include <windows.h>

int main(void)
{
    const char *name = "154_stream64.tmp";
    long long big = (5LL << 30) + 123, r;
    LARGE_INTEGER size;
    fpos_t pos;
    FILE *f = fopen(name, "w+b");
    int ok;

    if (!f)
        return printf("cannot create %s\n", name), 1;
    fputs("abc", f);
    printf("_ftelli64 %lld\n", _ftelli64(f));
    printf("ftello64 %lld\n", (long long)ftello64(f));
    r = _fseeki64(f, big, SEEK_SET);
    printf("_fseeki64 %lld %lld\n", r, _ftelli64(f));
    r = fseeko64(f, 1 - big, SEEK_CUR);
    printf("fseeko64 %lld %lld\n", r, (long long)ftello64(f));
    fgetpos(f, &pos);
    printf("fgetpos %lld\n", (long long)pos);
    fflush(f);
    ok = GetFileSizeEx((HANDLE)_get_osfhandle(_fileno(f)), &size);
    printf("GetFileSizeEx %d %lld\n", ok, (long long)size.QuadPart);
    fclose(f);
    remove(name);
    return 0;
}
