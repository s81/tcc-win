/* a redefinition in the same scope must have the same content */
struct T { int a; };
struct T { long long a; };
int main(void) { return 0; }
