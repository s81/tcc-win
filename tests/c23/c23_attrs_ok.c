/* [[nodiscard]] / [[deprecated]] used correctly: no diagnostics */
[[nodiscard]] int f(void) { return 1; }
struct [[nodiscard]] R { int x; };
struct R g(void) { struct R r = {1}; return r; }
[[deprecated("unused")]] int dead(void) { return 2; }
int main(void)
{
    int a = f();
    struct R r = g();
    (void)f();
    (void)g();
    return a + r.x - 2;
}
