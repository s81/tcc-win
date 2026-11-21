/* C23: letters outside ASCII are fine in identifiers */
int main(void)
{
    int été = 1, 日本語 = 2;
    return été + 日本語 - 3;
}
