#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
long __tcc_strtol(const char*,char**,int); unsigned long __tcc_strtoul(const char*,char**,int);
long long __tcc_strtoll(const char*,char**,int); unsigned long long __tcc_strtoull(const char*,char**,int);
long long __tcc_wcstoll(const wchar_t*,wchar_t**,int); unsigned long long __tcc_wcstoull(const wchar_t*,wchar_t**,int);
long long strtoimax(const char*,char**,int); long long wcstoll(const wchar_t*,wchar_t**,int);
int fails;
#define CHECK(e) do { if (!(e)) { printf("FAIL line %d: %s\n", __LINE__, #e); fails++; } } while (0)
int main(void){
  char *e; wchar_t *we;
  CHECK(__tcc_strtoll("0b101",&e,0)==5 && *e==0);
  CHECK(__tcc_strtoll("0B11",&e,2)==3 && *e==0);
  CHECK(__tcc_strtoll("-0b11",&e,0)==-3);
  CHECK(__tcc_strtoll("0b",&e,0)==0 && *e=='b');
  CHECK(__tcc_strtoll("0b2",&e,0)==0 && *e=='b');
  CHECK(__tcc_strtoll("0x1F",&e,0)==31 && __tcc_strtoll("0x1F",&e,16)==31);
  CHECK(__tcc_strtoll("0xZ",&e,0)==0 && *e=='x');
  CHECK(__tcc_strtoll("  \t 077",&e,0)==63 && __tcc_strtoll("12abc",&e,10)==12 && *e=='a');
  CHECK(__tcc_strtoll("zz",&e,36)==35*36+35);
  CHECK(__tcc_strtoll("abc",&e,0)==0 && e==(char*)0+0 || 1);
  { const char *s="abc"; __tcc_strtoll(s,&e,0); CHECK(e==s); }
  errno=0; CHECK(__tcc_strtoll("9223372036854775808",&e,10)==0x7fffffffffffffffLL && errno==ERANGE);
  errno=0; CHECK(__tcc_strtoll("-9223372036854775808",&e,10)==(-0x7fffffffffffffffLL-1) && errno==0);
  errno=0; CHECK(__tcc_strtoll("-9223372036854775809",&e,10)==(-0x7fffffffffffffffLL-1) && errno==ERANGE);
  errno=0; CHECK(__tcc_strtoull("18446744073709551615",&e,10)==~0ULL && errno==0);
  errno=0; CHECK(__tcc_strtoull("18446744073709551616",&e,10)==~0ULL && errno==ERANGE);
  CHECK(__tcc_strtoull("-1",&e,10)==~0ULL);
  errno=0; CHECK(__tcc_strtoll("1",&e,1)==0 && errno==EINVAL);
  CHECK(__tcc_strtol("0b1111",&e,0)==15);
  CHECK(__tcc_wcstoll(L"0b101",&we,0)==5 && *we==0);
  CHECK(__tcc_wcstoull(L" 0x10xyz",&we,0)==16 && *we==L'x');
  CHECK(strtoimax("0b101",&e,0)==0 && *e=='b');  /* C17 meaning */
  CHECK(strtoimax("0b101",&e,2)==0 && *e=='b');
  CHECK(wcstoll(L"0b1",&we,0)==0);
  puts(fails?"FAILED":"strto ok"); return fails; }
