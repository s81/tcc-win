int f();
int g(void) { return f(1); } /* error: () is (void) in C23 */
