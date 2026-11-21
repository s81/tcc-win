/* C23 6.4.2: universal character names in identifiers; \u00e9 and the UTF-8
   spelling are the same identifier */
#include <stdio.h>

#define M\u00e9 7
int \u00e9t\u00e9 = 3;
int a\u00e9b(void) { return 5; }
int \u65e5\u672c = 9;

int main(void)
{
    int x\
\u00e9y = 2;
    int x\u00e9y2 = 6;

    return !(M\u00e9 == 7 && \u00e9t\u00e9 == 3 && a\u00e9b() == 5
             && \u65e5\u672c == 9 && x\u00e9y == 2 && x\u00e9y2 == 6
             && Mé == 7 && été == 3 && aéb() == 5 && 日本 == 9
             && xéy == 2 && xéy2 == 6);
}
