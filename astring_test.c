#include "astring.c"

#include "utils.h"

static int errors = 0;

int main(void)
{
    astring str1 = { 0 };
    astring str2 = { 0 };
    astring *str3;

    // ** asRewind

    asRewind(&str1);

    make_sure_that(asLen(&str1) == 0);
    make_sure_that(asIsEmpty(&str1));

    // ** The asAdd* family

    asAdd(&str1, "ABCDEF", 3);

    make_sure_that(asLen(&str1) == 3);
    make_sure_that(!asIsEmpty(&str1));
    make_sure_that(strcmp(asGet(&str1), "ABC") == 0);

    asAddC(&str1, 'D');

    make_sure_that(asLen(&str1) == 4);
    make_sure_that(strcmp(asGet(&str1), "ABCD") == 0);

    asAddF(&str1, "%d", 1234);

    make_sure_that(asLen(&str1) == 8);
    make_sure_that(strcmp(asGet(&str1), "ABCD1234") == 0);

    asAddS(&str1, "XYZ");

    make_sure_that(asLen(&str1) == 11);
    make_sure_that(strcmp(asGet(&str1), "ABCD1234XYZ") == 0);

    // ** Overflow the initial 16 allocated bytes.

    asAddF(&str1, "%s", "1234567890");

    make_sure_that(asLen(&str1) == 21);
    make_sure_that(strcmp(asGet(&str1), "ABCD1234XYZ1234567890") == 0);

    // ** The asSet* family

    asSet(&str1, "ABCDEF", 3);

    make_sure_that(asLen(&str1) == 3);
    make_sure_that(strcmp(asGet(&str1), "ABC") == 0);

    asSetC(&str1, 'D');

    make_sure_that(asLen(&str1) == 1);
    make_sure_that(strcmp(asGet(&str1), "D") == 0);

    asSetF(&str1, "%d", 1234);

    make_sure_that(asLen(&str1) == 4);
    make_sure_that(strcmp(asGet(&str1), "1234") == 0);

    asSetS(&str1, "ABCDEF");

    make_sure_that(asLen(&str1) == 6);
    make_sure_that(strcmp(asGet(&str1), "ABCDEF") == 0);

    // ** asRewind again

    asRewind(&str1);

    make_sure_that(asLen(&str1) == 0);
    make_sure_that(strcmp(asGet(&str1), "") == 0);

    // ** asCat

    asSet(&str1, "ABC", 3);
    asSet(&str2, "DEF", 3);

    str3 = asCat(&str1, &str2);

    make_sure_that(&str1 == str3);

    make_sure_that(asLen(&str1) == 6);
    make_sure_that(strcmp(asGet(&str1), "ABCDEF") == 0);

    make_sure_that(asLen(&str2) == 3);
    make_sure_that(strcmp(asGet(&str2), "DEF") == 0);

    // ** asFinish

    char *r;

    // Regular string
    str3 = asCreate("ABCDEF");
    make_sure_that(strcmp((r = asFinish(str3)), "ABCDEF") == 0);
    free(r);

    // Empty astring (where str->data == NULL)
    str3 = asCreate(NULL);
    make_sure_that(strcmp((r = asFinish(str3)), "") == 0);
    free(r);

    // Reset astring (where str->data != NULL)
    str3 = asCreate("ABCDEF");
    asRewind(str3);
    make_sure_that(strcmp((r = asFinish(str3)), "") == 0);
    free(r);

    // ** asStrip

    asSetF(&str1, "ABCDEF");

    make_sure_that(strcmp(asGet(asStrip(&str1, 0, 0)), "ABCDEF") == 0);
    make_sure_that(strcmp(asGet(asStrip(&str1, 1, 0)), "BCDEF") == 0);
    make_sure_that(strcmp(asGet(asStrip(&str1, 0, 1)), "BCDE") == 0);
    make_sure_that(strcmp(asGet(asStrip(&str1, 1, 1)), "CD") == 0);
    make_sure_that(strcmp(asGet(asStrip(&str1, 3, 3)), "") == 0);

    // ** asStartsWith, asEndsWith

    asSetS(&str1, "abcdef");

    make_sure_that(asStartsWith(&str1, "abc") == true);
    make_sure_that(asStartsWith(&str1, "def") == false);
    make_sure_that(asEndsWith(&str1, "def") == true);
    make_sure_that(asEndsWith(&str1, "abc") == false);

    make_sure_that(asStartsWith(&str1, "%s", "abc") == true);
    make_sure_that(asStartsWith(&str1, "%s", "def") == false);
    make_sure_that(asEndsWith(&str1, "%s", "def") == true);
    make_sure_that(asEndsWith(&str1, "%s", "abc") == false);

    asClear(&str1);

    asSetS(&str1, "123456789");

    make_sure_that(asStartsWith(&str1, "123") == true);
    make_sure_that(asStartsWith(&str1, "789") == false);
    make_sure_that(asEndsWith(&str1, "789") == true);
    make_sure_that(asEndsWith(&str1, "123") == false);

    make_sure_that(asStartsWith(&str1, "%d", 123) == true);
    make_sure_that(asStartsWith(&str1, "%d", 789) == false);
    make_sure_that(asEndsWith(&str1, "%d", 789) == true);
    make_sure_that(asEndsWith(&str1, "%d", 123) == false);

    // ** asSetT and asAddT.

    asSetT(&str1, 1660842836, "Europe/Amsterdam", "%Y-%m-%d");

    make_sure_that(asLen(&str1) == 10);
    make_sure_that(strcmp(asGet(&str1), "2022-08-18") == 0);

    asAddT(&str1, 1660842836, "Europe/Amsterdam", " %H:%M:%S");

    make_sure_that(asLen(&str1) == 19);
    make_sure_that(strcmp(asGet(&str1), "2022-08-18 19:13:56") == 0);

    asSetT(&str1, 1660842836, "UTC", "%Y-%m-%d");

    make_sure_that(asLen(&str1) == 10);
    make_sure_that(strcmp(asGet(&str1), "2022-08-18") == 0);

    asAddT(&str1, 1660842836, "UTC", " %H:%M:%S");

    make_sure_that(asLen(&str1) == 19);
    make_sure_that(strcmp(asGet(&str1), "2022-08-18 17:13:56") == 0);

    asClear(&str1);

    return errors;
}
