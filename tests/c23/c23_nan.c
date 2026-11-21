/* NAN is a positive quiet NaN of type float, usable in constant
   initializers; (0.0F/0.0F) used to give a negative one (printed "-nan") */
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

static const float gf = NAN;
static const double gd = NAN;
static_assert(_Generic(NAN, float: 1, default: 0));

int main(void)
{
    char b[32];
    float f = NAN;
    CHECK(isnan(f) && isnan(gf) && isnan(gd));
    CHECK(!signbit(f) && !signbit(gf) && !signbit(gd));
    CHECK(!issignaling(f));
    snprintf(b, sizeof b, "%f %F", (double)f, gd);
    CHECK(!strcmp(b, "nan NAN"));
    return 0;
}
