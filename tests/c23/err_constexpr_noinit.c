/* a constexpr object must be initialized */
struct P { int x, y; };
constexpr struct P p;
int main(void) { return 0; }
