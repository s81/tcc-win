/* '##' inside a stringized __VA_OPT__ */
#define G(X, ...) #__VA_OPT__(X##X __VA_ARGS__)
#define K(X, ...) #__VA_OPT__(X ## _ ## X)
_Static_assert(sizeof G(a, b) == sizeof "aa b", "paste before stringize");
_Static_assert(sizeof G(a) == sizeof "", "empty");
_Static_assert(sizeof K(a, 1) == sizeof "a_a", "paste chain");
int main(void) { return 0; }
