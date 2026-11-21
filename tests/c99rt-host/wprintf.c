#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdarg.h>
int __tcc_swprintf(wchar_t*,size_t,const wchar_t*,...);
int __tcc_vswprintf(wchar_t*,size_t,const wchar_t*,va_list);
int __tcc_fwprintf(FILE*,const wchar_t*,...);
int fails;
#define CHECK(e) do { if (!(e)) { printf("FAIL line %d: %s\n", __LINE__, #e); fails++; } } while (0)
#define EQ(fmt, ...) do { wchar_t b[128]; int r = __tcc_swprintf(b,128,fmt,__VA_ARGS__); if (wcscmp(b, EXP)) { printf("FAIL line %d: got [%ls] r=%d\n", __LINE__, b, r); fails++; } } while(0)
int main(void){
  wchar_t b[64];
#define EXP L"101|  101|0b101|-7|hi"
  EQ(L"%b|%5B|%#b|%d|%ls", 5, 5, 5, -7, L"hi");
#undef EXP
#define EXP L"00101|101  |0B101"
  EQ(L"%05b|%-5b|%#B", 5, 5, 5);
#undef EXP
#define EXP L"x:[ab]:[  ab]:[ab  ]:7"
  EQ(L"x:[%ls]:[%4ls]:[%-4ls]:%d", L"ab", L"ab", L"ab", 7);
#undef EXP
#define EXP L"1.5 2.5e+10 inf 1e+02"
  EQ(L"%.1f %g %f %.0e", 1.5, 2.5e10, 1.0/0.0 > 0 ? __builtin_inf() : 0.0, 100.0);
#undef EXP
#define EXP L"[abc][x][ A]%"
  EQ(L"[%hs][%.1hs][%2lc]%%", "abc", "xyz", L'A');
  CHECK(__tcc_swprintf(b, 4, L"%ls", L"abcdef") == -1 && !wcscmp(b, L"abc"));
  CHECK(__tcc_swprintf(b, 4, L"%ls", L"abc") == 3);
  CHECK(__tcc_swprintf(b, 1, L"x") == -1 && b[0]==0);
  { int n=0; __tcc_swprintf(b, 64, L"abc%n", &n); CHECK(n==3); }
  { long long v = 0x123456789LL; __tcc_swprintf(b, 64, L"%llx %I64d %zu", v, v, (size_t)3); CHECK(!wcscmp(b, L"123456789 4886718345 3")); }
  { __tcc_swprintf(b, 64, L"%*d|%-*d|%.*f", 4, 5, 4, 6, 2, 3.14159); CHECK(!wcscmp(b, L"   5|6   |3.14")); }
  puts(fails ? "FAILED" : "wprintf ok"); return fails; }
