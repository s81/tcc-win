/* C11/C23: u'c', U'c', u"s", U"s", u8"s" with their element types */
typedef unsigned short c16;
typedef unsigned int c32;

#define CHECK(e) do { if (!(e)) return __LINE__; } while (0)

int main(void)
{
    c16 s16[] = u"h\u00e9\U0001F600";   /* é, then a surrogate pair */
    c32 s32[] = U"h\u00e9\U0001F600";
    unsigned char s8[] = u8"h\u00e9";
    c32 cat[] = U"a" "b" U"c";
    c32 *p = U"zz";

    CHECK(u'a' == 97 && U'a' == 97 && u'\u00e9' == 0xe9);
    CHECK(U'\U0001F600' == 0x1F600);
    CHECK(sizeof(u'a') == 2 && sizeof(U'a') == 4);
    CHECK(_Generic(u'a', unsigned short: 1, default: 0));
    CHECK(_Generic(U'a', unsigned int: 1, default: 0));
    CHECK(_Generic(u8'a', unsigned char: 1, default: 0));
    CHECK(_Generic(u8"a", unsigned char *: 1, default: 0));
    CHECK(_Generic(u"a", c16 *: 1, default: 0));
    CHECK(_Generic(U"a", c32 *: 1, default: 0));
    CHECK(sizeof s16 == 5 * 2 && s16[1] == 0xe9);
    CHECK(s16[2] == 0xD83D && s16[3] == 0xDE00 && s16[4] == 0);
    CHECK(sizeof s32 == 4 * 4 && s32[2] == 0x1F600);
    CHECK(sizeof s8 == 4 && s8[1] == 0xc3 && s8[2] == 0xa9);
    CHECK(sizeof cat == 4 * 4 && cat[1] == 'b' && cat[2] == 'c');
    CHECK(p[1] == 'z' && p[2] == 0);
    CHECK(u"abc"[1] == 'b' && sizeof(u"abc") == 8 && sizeof(U"abc") == 16);
#define STR(x) #x
#define XSTR(x) STR(x)
    CHECK(XSTR(u'a')[0] == 'u' && XSTR(U"a")[0] == 'U');
    return 0;
}
