int f(int x)
{
    while (x) {
        if (x == 3)
            goto done; /* error */
        --x;
    }
done:
    return x;
}
