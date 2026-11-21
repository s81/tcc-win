#include <fenv.h>
#include <stdio.h>
#define CHECK(e) do { if (!(e)) { printf("FAIL line %d: %s\n", __LINE__, #e); fails++; } } while (0)
int fails;
int main(void){
  fenv_t env; fexcept_t fl; femode_t md;
  CHECK(feclearexcept(FE_ALL_EXCEPT)==0 && fetestexcept(FE_ALL_EXCEPT)==0);
  CHECK(feraiseexcept(FE_INEXACT|FE_OVERFLOW)==0);
  CHECK(fetestexcept(FE_ALL_EXCEPT)==(FE_INEXACT|FE_OVERFLOW));
  CHECK(fetestexcept(FE_INEXACT)==FE_INEXACT && fetestexcept(FE_DIVBYZERO)==0);
  CHECK(fegetexceptflag(&fl, FE_INEXACT)==0 && fl==FE_INEXACT);
  CHECK(feclearexcept(FE_ALL_EXCEPT)==0 && fetestexcept(FE_ALL_EXCEPT)==0);
  CHECK(fesetexceptflag(&fl, FE_ALL_EXCEPT)==0 && fetestexcept(FE_ALL_EXCEPT)==FE_INEXACT);
  CHECK(fetestexceptflag(&fl, FE_INEXACT|FE_OVERFLOW)==FE_INEXACT);
  CHECK(feclearexcept(0x100)!=0);
  CHECK(fegetround()==FE_TONEAREST);
  CHECK(fesetround(FE_UPWARD)==0 && fegetround()==FE_UPWARD);
  { volatile double a = 1.0, b = 3.0; volatile double up = a/b; fesetround(FE_DOWNWARD); volatile double dn = a/b; CHECK(up > dn); }
  CHECK(fesetround(0x1234)!=0 && fegetround()==FE_DOWNWARD);
  fesetround(FE_TOWARDZERO);
  CHECK(fegetmode(&md)==0);
  fesetround(FE_TONEAREST);
  CHECK(fesetmode(&md)==0 && fegetround()==FE_TOWARDZERO);
  CHECK(fesetmode(FE_DFL_MODE)==0 && fegetround()==FE_TONEAREST);
  feclearexcept(FE_ALL_EXCEPT); feraiseexcept(FE_UNDERFLOW);
  CHECK(fegetenv(&env)==0);
  fesetround(FE_UPWARD); feraiseexcept(FE_DIVBYZERO);
  CHECK(fesetenv(&env)==0 && fegetround()==FE_TONEAREST && fetestexcept(FE_ALL_EXCEPT)==FE_UNDERFLOW);
  CHECK(feholdexcept(&env)==0 && fetestexcept(FE_ALL_EXCEPT)==0);
  feraiseexcept(FE_INEXACT);
  CHECK(feupdateenv(&env)==0 && fetestexcept(FE_ALL_EXCEPT)==(FE_UNDERFLOW|FE_INEXACT));
  CHECK(fesetenv(FE_DFL_ENV)==0 && fegetround()==FE_TONEAREST);
  puts(fails ? "FAILED" : "fenv ok");
  return fails;
}
