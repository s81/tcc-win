/* the initializer of a constexpr object must be a constant expression */
struct P { int x, y; };
int main(int argc, char **argv)
{
    (void)argv;
    constexpr struct P p = { argc, 2 };
    return p.y;
}
