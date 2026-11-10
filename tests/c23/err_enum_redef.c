/* an enum redefinition must have the same enumerators and values */
enum E { A, B };
enum E { A, B = 2 };
int main(void) { return 0; }
