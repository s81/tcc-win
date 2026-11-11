[[nodiscard("check it")]] int f(void) { return 1; }
struct [[nodiscard]] R { int x; };
struct R g(void) { struct R r = {1}; return r; }
[[deprecated("use h2")]] int h(void) { return 2; }
[[deprecated]] int gv = 3;
[[deprecated("old type")]] typedef int OLD;
void k(void) {
    f();
    (void)f();
    int a = f();
    g();
    h();
    OLD z = gv;
    -f();
    f() + 1;
    (void)a; (void)z;
}
int main(void) { k(); return 0; }
