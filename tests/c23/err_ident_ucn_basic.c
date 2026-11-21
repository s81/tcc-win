/* C23 6.4.3: a universal character name must not name a basic character */
int main(void)
{
    int a\u0041 = 1;
    return 0;
}
