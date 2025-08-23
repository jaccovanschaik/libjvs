#include "wstring.c"
#include "utils.h"

static int errors = 0;

int main(void)
{
    wstring str1 = { 0 };
    wstring str2 = { 0 };
    wstring *str3;

    wsInit(&str1, NULL);

    make_sure_that(wsLen(&str1) == 0);
    make_sure_that(wsIsEmpty(&str1));

    // ** wsRewind

    wsRewind(&str1);

    make_sure_that(wsLen(&str1) == 0);
    make_sure_that(wsIsEmpty(&str1));

    // ** The wsAdd* family

    wsAdd(&str1, L"ABCDEF", 3);

    make_sure_that(wsLen(&str1) == 3);
    make_sure_that(!wsIsEmpty(&str1));
    make_sure_that(wcscmp(wsGet(&str1), L"ABC") == 0);

    // Add a single wide character...

    wsAddC(&str1, L'D');

    make_sure_that(wsLen(&str1) == 4);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCD") == 0);

    // Add a formatted number...

    wsAddF(&str1, L"%d", 12);

    make_sure_that(wsLen(&str1) == 6);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCD12") == 0);

    // Add a formatted ASCII string...

    wsAddF(&str1, L"%s", "3");

    make_sure_that(wsLen(&str1) == 7);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCD123") == 0);

    // Add a formatted wide string...

    wsAddF(&str1, L"%ls", L"4");

    make_sure_that(wsLen(&str1) == 8);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCD1234") == 0);

    // Add a null-terminated wide string...

    wsAddS(&str1, L"XYZ");

    make_sure_that(wsLen(&str1) == 11);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCD1234XYZ") == 0);

    // ** Overflow the initial 16 allocated bytes.

    wsAddF(&str1, L"%ls", L"1234567890");

    make_sure_that(wsLen(&str1) == 21);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCD1234XYZ1234567890") == 0);

    // ** The wsSet* family

    wsSet(&str1, L"ABCDEF", 3);

    make_sure_that(wsLen(&str1) == 3);
    make_sure_that(wcscmp(wsGet(&str1), L"ABC") == 0);

    wsSetC(&str1, L'D');

    make_sure_that(wsLen(&str1) == 1);
    make_sure_that(wcscmp(wsGet(&str1), L"D") == 0);

    wsSetF(&str1, L"%d", 1234);

    make_sure_that(wsLen(&str1) == 4);
    make_sure_that(wcscmp(wsGet(&str1), L"1234") == 0);

    wsSetS(&str1, L"ABCDEF");

    make_sure_that(wsLen(&str1) == 6);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCDEF") == 0);

    // ** wsRewind again

    wsRewind(&str1);

    make_sure_that(wsLen(&str1) == 0);
    make_sure_that(wcscmp(wsGet(&str1), L"") == 0);

    // ** wsCat

    wsSet(&str1, L"ABC", 3);
    wsSet(&str2, L"DEF", 3);

    str3 = wsCat(&str1, &str2);

    make_sure_that(&str1 == str3);

    make_sure_that(wsLen(&str1) == 6);
    make_sure_that(wcscmp(wsGet(&str1), L"ABCDEF") == 0);

    make_sure_that(wsLen(&str2) == 3);
    make_sure_that(wcscmp(wsGet(&str2), L"DEF") == 0);

    // ** wsFinish

    wchar_t *r;

    // Regular string
    str3 = wsCreate(L"ABCDEF");
    make_sure_that(wcscmp((r = wsFinish(str3)), L"ABCDEF") == 0);
    free(r);

    // Empty wstring (where str->data == NULL)
    str3 = wsCreate(NULL);
    make_sure_that(wcscmp((r = wsFinish(str3)), L"") == 0);
    free(r);

    // Reset wstring (where str->data != NULL)
    str3 = wsCreate(L"ABCDEF");
    wsRewind(str3);
    make_sure_that(wcscmp((r = wsFinish(str3)), L"") == 0);
    free(r);

    // ** wsStrip

    wsSetF(&str1, L"ABCDEF");

    make_sure_that(wcscmp(wsGet(wsStrip(&str1, 0, 0)), L"ABCDEF") == 0);
    make_sure_that(wcscmp(wsGet(wsStrip(&str1, 1, 0)), L"BCDEF") == 0);
    make_sure_that(wcscmp(wsGet(wsStrip(&str1, 0, 1)), L"BCDE") == 0);
    make_sure_that(wcscmp(wsGet(wsStrip(&str1, 1, 1)), L"CD") == 0);
    make_sure_that(wcscmp(wsGet(wsStrip(&str1, 3, 3)), L"") == 0);

    // ** wsStartsWith, wsEndsWith

    wsSetS(&str1, L"abcdef");

    make_sure_that(wsStartsWith(&str1, L"abc") == true);
    make_sure_that(wsStartsWith(&str1, L"def") == false);
    make_sure_that(wsEndsWith(&str1, L"def") == true);
    make_sure_that(wsEndsWith(&str1, L"abc") == false);

    make_sure_that(wsStartsWith(&str1, L"%ls", L"abc") == true);
    make_sure_that(wsStartsWith(&str1, L"%ls", L"def") == false);
    make_sure_that(wsEndsWith(&str1, L"%ls", L"def") == true);
    make_sure_that(wsEndsWith(&str1, L"%ls", L"abc") == false);

    make_sure_that(wsStartsWith(&str1, L"%s", "abc") == true);
    make_sure_that(wsStartsWith(&str1, L"%s", "def") == false);
    make_sure_that(wsEndsWith(&str1, L"%s", "def") == true);
    make_sure_that(wsEndsWith(&str1, L"%s", "abc") == false);

    wsClear(&str1);

    wsSetS(&str1, L"123456789");

    make_sure_that(wsStartsWith(&str1, L"123") == true);
    make_sure_that(wsStartsWith(&str1, L"789") == false);
    make_sure_that(wsEndsWith(&str1, L"789") == true);
    make_sure_that(wsEndsWith(&str1, L"123") == false);

    make_sure_that(wsStartsWith(&str1, L"%d", 123) == true);
    make_sure_that(wsStartsWith(&str1, L"%d", 789) == false);
    make_sure_that(wsEndsWith(&str1, L"%d", 789) == true);
    make_sure_that(wsEndsWith(&str1, L"%d", 123) == false);

    // ** wsSetT and wsAddT.

    wsSetT(&str1, 1660842836, "Europe/Amsterdam", L"%Y-%m-%d");

    make_sure_that(wsLen(&str1) == 10);
    make_sure_that(wcscmp(wsGet(&str1), L"2022-08-18") == 0);

    wsAddT(&str1, 1660842836, "Europe/Amsterdam", L" %H:%M:%S");

    make_sure_that(wsLen(&str1) == 19);
    make_sure_that(wcscmp(wsGet(&str1), L"2022-08-18 19:13:56") == 0);

    wsSetT(&str1, 1660842836, "UTC", L"%Y-%m-%d");

    make_sure_that(wsLen(&str1) == 10);
    make_sure_that(wcscmp(wsGet(&str1), L"2022-08-18") == 0);

    wsAddT(&str1, 1660842836, "UTC", L" %H:%M:%S");

    make_sure_that(wsLen(&str1) == 19);
    make_sure_that(wcscmp(wsGet(&str1), L"2022-08-18 17:13:56") == 0);

    wsClear(&str1);

    const char *utf8_txt = "αß¢";
    size_t utf8_len = strlen(utf8_txt);

    wsFromUtf8(&str1, utf8_txt, utf8_len);

    make_sure_that(wcscmp(wsGet(&str1), L"αß¢") == 0);

    wsSetS(&str1, L"Smørrebrød i københavn");

    utf8_txt = wsToUtf8(&str1, &utf8_len);

    make_sure_that(strcmp(utf8_txt, "Smørrebrød i københavn") == 0);

    return errors;
}
