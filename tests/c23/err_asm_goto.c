void f(void)
{
    asm goto ("" :::: out); /* error */
out:
    ;
}
