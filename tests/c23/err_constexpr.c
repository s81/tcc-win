constexpr int N = 1;
int g(void) { extern int x; constexpr int M = x; return M + N; }
