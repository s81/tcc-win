#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
#include <stddef.h>
int __tcc_sscanf(const char*,const char*,...); int __tcc_swscanf(const wchar_t*,const wchar_t*,...);
int __tcc_fscanf(FILE*,const char*,...);
int fails;
#define CHECK(e) do { if (!(e)) { printf("FAIL line %d: %s\n", __LINE__, #e); fails++; } } while (0)
int main(void){
  int i, j, n; unsigned u; long long ll; short sh; signed char hh; char s[32], c, s2[8]; float f; double d; unsigned long ul;
  CHECK(__tcc_sscanf("101","%b",&i)==1 && i==5);
  CHECK(__tcc_sscanf("0b110 7","%b %d",&i,&j)==2 && i==6 && j==7);
  CHECK(__tcc_sscanf("0B11","%B",&i)==1 && i==3);
  CHECK(__tcc_sscanf("0b11 0x1f 077 12","%i %i %i %i",&i,&j,&n,&u)==4 && i==3 && j==31 && n==63 && u==12);
  CHECK(__tcc_sscanf("-42 +7 99","%d %d %hhd",&i,&j,&hh)==3 && i==-42 && j==7 && hh==99);
  { unsigned char guard[4]={1,2,3,4}; signed char v; (void)guard; CHECK(__tcc_sscanf("300","%hhd",&v)==1 && v==(signed char)300 && guard[0]==1); }
  CHECK(__tcc_sscanf("123456789012","%lld",&ll)==1 && ll==123456789012LL);
  CHECK(__tcc_sscanf("65535","%hu",&sh)==1 && (unsigned short)sh==65535);
  CHECK(__tcc_sscanf("ff","%x",&u)==1 && u==255);
  CHECK(__tcc_sscanf("17","%o",&u)==1 && u==15);
  CHECK(__tcc_sscanf("hello world","%s %c",s,&c)==2 && !strcmp(s,"hello") && c=='w');
  CHECK(__tcc_sscanf("abc123","%3s%d",s,&i)==2 && !strcmp(s,"abc") && i==123);
  CHECK(__tcc_sscanf("aabbcc dd","%[ab]%n",s,&n)==1 && !strcmp(s,"aabb") && n==4);
  CHECK(__tcc_sscanf("xyz;rest","%[^;]",s)==1 && !strcmp(s,"xyz"));
  CHECK(__tcc_sscanf("a-d-z","%[a-d-]",s)==1 && !strcmp(s,"a-d-"));
  CHECK(__tcc_sscanf("1.5 2.25e2 inf nan -0x1p3","%f %lf %f %f %lf",&f,&d,&f,&f,&d)==5 && d==-8.0);
  CHECK(__tcc_sscanf("1.5","%f",&f)==1 && f==1.5f);
  CHECK(__tcc_sscanf("2.5e1x","%lf%c",&d,&c)==2 && d==25.0 && c=='x');
  CHECK(__tcc_sscanf("12 34","%*d %d",&i)==1 && i==34);
  CHECK(__tcc_sscanf("abc","%d",&i)==0);
  CHECK(__tcc_sscanf("","%d",&i)==-1);
  CHECK(__tcc_sscanf("   ","%d",&i)==-1);
  CHECK(__tcc_sscanf("5 abc","%d x",&i)==1);
  CHECK(__tcc_sscanf("10%","%d%%",&i)==1 && i==10);
  CHECK(__tcc_sscanf("a b","%c%c%c",&c,&c,&c)==3);
  CHECK(__tcc_sscanf("ab","%c%c%c",&c,&c,&c)==2);
  CHECK(__tcc_sscanf("0x10","%p",(void**)&ul)==1 && ul==16);
  CHECK(__tcc_sscanf("12345","%2d%d",&i,&j)==2 && i==12 && j==345);
  CHECK(__tcc_sscanf("42abc","%d%n",&i,&n)==1 && n==2);
  CHECK(__tcc_sscanf("lit:5","lit:%d",&i)==1 && i==5);
  CHECK(__tcc_sscanf("lit:5","lix:%d",&i)==0);
  { size_t z; ptrdiff_t t; intmax_t jm; CHECK(__tcc_sscanf("1 2 3","%zu %td %jd",&z,&t,&jm)==3 && z==1 && t==2 && jm==3); }
  { wchar_t w[8]; wchar_t wc;
    CHECK(__tcc_swscanf(L"abc 12 0b11",L"%ls %d %b",w,&i,&j)==3 && !wcscmp(w,L"abc") && i==12 && j==3);
    CHECK(__tcc_swscanf(L"x",L"%lc",&wc)==1 && wc==L'x');
    CHECK(__tcc_swscanf(L"hello 5",L"%s %d",w,&i)==2 && !wcscmp(w,L"hello") && i==5);
    CHECK(__tcc_swscanf(L"1.25",L"%lf",&d)==1 && d==1.25);
    CHECK(__tcc_swscanf(L"",L"%d",&i)==-1);
    CHECK(__tcc_sscanf("hello","%ls",w)==1);
  }
  { FILE *fp = tmpfile(); fputs("7 0b11 word\nnext", fp); rewind(fp);
    CHECK(__tcc_fscanf(fp,"%d %b %s",&i,&j,s)==3 && i==7 && j==3 && !strcmp(s,"word"));
    CHECK(__tcc_fscanf(fp," %s",s)==1 && !strcmp(s,"next"));
    CHECK(__tcc_fscanf(fp,"%d",&i)==-1);
    fclose(fp); }
  puts(fails ? "FAILED" : "scanf ok"); return fails; }
