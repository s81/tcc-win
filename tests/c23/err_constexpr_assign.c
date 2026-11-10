/* constexpr objects are read-only */
struct P { int x, y; };
constexpr struct P p = { 1, 2 };
int main(void) { struct P *q = (struct P *)&p; q->x = 0; p.x = 3; return 0; }
