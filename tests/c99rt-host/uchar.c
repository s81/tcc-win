#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <errno.h>
size_t mbrtoc16(unsigned short*,const char*,size_t,mbstate_t*); size_t c16rtomb(char*,unsigned short,mbstate_t*);
size_t mbrtoc32(unsigned*,const char*,size_t,mbstate_t*); size_t c32rtomb(char*,unsigned,mbstate_t*);
size_t mbrtoc8(unsigned char*,const char*,size_t,mbstate_t*); size_t c8rtomb(char*,unsigned char,mbstate_t*);
int fails;
#define CHECK(e) do { if (!(e)) { printf("FAIL line %d: %s\n", __LINE__, #e); fails++; } } while (0)
int main(void){
  mbstate_t st; memset(&st,0,sizeof st); unsigned short c16; unsigned c32; unsigned char c8; char out[8];
  if (!setlocale(LC_ALL,"C.UTF-8") && !setlocale(LC_ALL,"en_US.UTF-8")) { puts("no UTF-8 locale; skipping"); return 0; }
  CHECK(mbrtoc16(&c16,"a",1,&st)==1 && c16=='a');
  CHECK(mbrtoc16(&c16,"\xc3\xa9",2,&st)==2 && c16==0xe9);
  CHECK(mbrtoc16(&c16,"\xe2\x82\xac",3,&st)==3 && c16==0x20ac);
  CHECK(mbrtoc16(&c16,"\xe2\x82",2,&st)==(size_t)-2);
  CHECK(mbrtoc16(&c16,"\xac!",2,&st)==1 && c16==0x20ac);   /* completes with 1 byte of the new input */
  CHECK(mbrtoc16(&c16,"",1,&st)==0 && c16==0);
  errno=0; CHECK(mbrtoc16(&c16,"\xff",1,&st)==(size_t)-1 && errno==EILSEQ);
  CHECK(mbrtoc32(&c32,"\xe2\x82\xac",3,&st)==3 && c32==0x20ac);
  CHECK(c16rtomb(out,0x20ac,&st)==3 && !memcmp(out,"\xe2\x82\xac",3));
  CHECK(c16rtomb(out,'x',&st)==1 && out[0]=='x');
  CHECK(c32rtomb(out,0xe9,&st)==2 && !memcmp(out,"\xc3\xa9",2));
  errno=0; CHECK(c32rtomb(out,0x1F600,&st)==(size_t)-1 && errno==EILSEQ);
  memset(&st,0,sizeof st);
  CHECK(mbrtoc8(&c8,"\xe2\x82\xac",3,&st)==3 && c8==0xe2);
  CHECK(mbrtoc8(&c8,"",0,&st)==(size_t)-3 && c8==0x82);
  CHECK(mbrtoc8(&c8,"",0,&st)==(size_t)-3 && c8==0xac);
  CHECK(mbrtoc8(&c8,"a",1,&st)==1 && c8=='a');
  CHECK(mbrtoc8(&c8,"\xc3\xa9",2,&st)==2 && c8==0xc3);
  CHECK(mbrtoc8(&c8,"",0,&st)==(size_t)-3 && c8==0xa9);
  CHECK(c8rtomb(out,0xe2,&st)==0 && c8rtomb(out,0x82,&st)==0 && c8rtomb(out,0xac,&st)==3 && !memcmp(out,"\xe2\x82\xac",3));
  CHECK(c8rtomb(out,'q',&st)==1 && out[0]=='q');
  CHECK(c8rtomb(out,0xc3,&st)==0 && c8rtomb(out,0xa9,&st)==2 && !memcmp(out,"\xc3\xa9",2));
  errno=0; CHECK(c8rtomb(out,0x80,&st)==(size_t)-1 && errno==EILSEQ);
  puts(fails?"FAILED":"uchar ok"); return fails; }
