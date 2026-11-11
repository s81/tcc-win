/* K&R definitions are accepted below C23 */
int add(a, b) int a; int b; { return a + b; }
int main(void) { return add(1, 2) - 3; }
