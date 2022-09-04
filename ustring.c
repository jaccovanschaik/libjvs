/*
 * ustring.c: Handle Unicode strings.
 *
 * ustring.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: ustring.c 463 2022-08-19 08:17:47Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <wchar.h>
#include <time.h>
#include <iconv.h>

#include "utils.h"
#include "debug.h"
#include "ustring.h"

#define INITIAL_SIZE 16

/*
 * Increase the size of <str> to allow for another <len> bytes.
 */
static void us_increase_by(ustring *str, size_t len)
{
    size_t new_len;

    while (str->used + len + 1 > str->size) {
        if (str->size == 0)
            new_len = INITIAL_SIZE;
        else
            new_len = 2 * str->size;

        str->data = realloc(str->data, new_len * sizeof(wchar_t));

        str->size = new_len;
    }
}

/*
 * Create an empty ustring.
 */
ustring *usCreate(void)
{
    ustring *str = calloc(1, sizeof(*str));

    return str;
}

/*
 * Initialize ustring <str>.
 */
ustring *usInit(ustring *str)
{
    str->size = INITIAL_SIZE;
    str->data = calloc(1, str->size * sizeof(wchar_t));
    str->used = 0;

    return str;
}

/*
 * Clear <str>, freeing its internal data. Use this if you have an automatically
 * allocated ustring and want to completely discard its contents before it goes
 * out of scope. If <str> was dynamically allocated (using usCreate() above)
 * you may free() it after calling this function (or you could simply call
 * usDestroy()).
 */
void usClear(ustring *str)
{
    free(str->data);

    memset(str, 0, sizeof(ustring));
}

/*
 * Detach the character string inside <str> and return it, and reinitialize
 * the ustring. The caller is responsible for the returned data after this and
 * should free() it when finished.
 */
wchar_t *usDetach(ustring *str)
{
    wchar_t *data = str->data;

    str->data = NULL;
    str->size = 0;
    str->used = 0;

    return data ? data : wcsdup(L"");
}

/*
 * Destroy <str>, but save and return its contents. The caller is responsible
 * for the returned data after this and should free() it when finished.
 */
wchar_t *usFinish(ustring *str)
{
    wchar_t *data = str->data;

    free(str);

    return data ? data : wcsdup(L"");
}

/*
 * Destroy <str>, together with the data it contains.
 */
void usDestroy(ustring *str)
{
    free(str->data);
    free(str);
}

/*
 * Add <len> bytes, starting at <data> to <str>.
 */
ustring *usAdd(ustring *str, const void *data, size_t len)
{
    us_increase_by(str, len);

    memcpy(str->data + str->used, data, len * sizeof(wchar_t));

    str->used += len;

    str->data[str->used] = '\0';

    return str;
}

/*
 * Add the single character <c> to <str>.
 */
ustring *usAddC(ustring *str, wchar_t c)
{
    usAdd(str, &c, 1);

    return str;
}

/*
 * Append a string to <str>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
ustring *usAddV(ustring *str, const wchar_t *fmt, va_list ap)
{
    va_list my_ap;

    while (true) {
        int r;
        size_t avail_size = str->size - str->used;

        va_copy(my_ap, ap);
        r = vswprintf(str->data + str->used, avail_size, fmt, my_ap);
        va_end(my_ap);

        if (r >= 0) {
            str->used += r;
            break;
        }

        us_increase_by(str, str->size);
    }

    return str;
}

/*
 * Append a string to <str>, formatted according to <fmt> and the subsequent
 * parameters. */
ustring *usAddF(ustring *str, const wchar_t *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    usAddV(str, fmt, ap);
    va_end(ap);

    return str;
}

/*
 * Append the null-terminated string <s> to <str>.
 */
ustring *usAddS(ustring *str, const wchar_t *s)
{
    return usAdd(str, s, wcslen(s));
}

/*
 * Append a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>. If
 * <tz> is NULL, the time zone in the environment variable TZ is used.
 */
ustring *usAddT(ustring *str, time_t t, const char *tz, const wchar_t *fmt)
{
    const char *old_tz = NULL;

    if (tz != NULL) {
        old_tz = getenv("TZ");
        setenv("TZ", tz, 1);
    }

    wchar_t *new_fmt = calloc(wcslen(fmt) + 2, sizeof(wchar_t));

    new_fmt[0] = L'x';
    wcscat(new_fmt, fmt);

    size_t buf_size = 32;
    wchar_t *buf = calloc(buf_size, sizeof(wchar_t));

    int r;

    struct tm *tm = localtime(&t);

    while ((r = wcsftime(buf, buf_size, new_fmt, tm)) == 0) {
        buf_size *= 2;

        buf = realloc(buf, buf_size);
    }

    if (old_tz != NULL) setenv("TZ", old_tz, 1);

    usAdd(str, buf + 1, wcslen(buf) - 1);

    free(new_fmt);
    free(buf);

    return str;
}

/*
 * Replace <str> with the <len> bytes starting at <data>.
 */
ustring *usSet(ustring *str, const void *data, size_t len)
{
    usRewind(str);

    return usAdd(str, data, len);
}

/*
 * Set <str> to the single character <c>.
 */
ustring *usSetC(ustring *str, wchar_t c)
{
    usRewind(str);

    return usAdd(str, &c, 1);
}

/*
 * Set <str> to a string formatted according to <fmt> and the subsequent
 * parameters.
 */
ustring *usSetF(ustring *str, const wchar_t *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    usSetV(str, fmt, ap);
    va_end(ap);

    return str;
}

/*
 * Replace <str> with a string formatted according to <fmt> and the subsequent
 * parameters contained in <ap>.
 */
ustring *usSetV(ustring *str, const wchar_t *fmt, va_list ap)
{
    usRewind(str);

    return usAddV(str, fmt, ap);
}

/*
 * Set <str> to the null-terminated string <str>.
 */
ustring *usSetS(ustring *str, const wchar_t *s)
{
    return usSet(str, s, wcslen(s));
}

/*
 * Write a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>.
 * If <tz> is NULL, the time zone in the environment variable TZ is used.
 */
ustring *usSetT(ustring *str, time_t t, const char *tz, const wchar_t *fmt)
{
    usRewind(str);

    return usAddT(str, t, tz, fmt);
}

/*
 * Fill <str> using the UTF-8 text in <utf8_text>, which is <utf8_len> bytes
 * (not necessarily characters!) long. Returns the same <str> that was passed
 * in.
 */
ustring *usFromUtf8(ustring *str, const char *utf8_txt, size_t utf8_len)
{
    static iconv_t conv = NULL;

    if (conv == NULL) {
        conv = iconv_open("WCHAR_T", "UTF-8");

        dbgAssert(stderr, conv != (iconv_t) -1,
                "Could not create converter from UTF-8 to wchar_t");
    }

    size_t wchar_len;
    const char *wchar_txt =
        convert_charset(conv, utf8_txt, utf8_len, &wchar_len);

    return usSet(str, wchar_txt, wchar_len / 4);
}

/*
 * Return a UTF-8 version of <str>. The number of bytes (not necessarily
 * characters!) in the UTF-8 string is returned through <len>. The returned
 * pointer points to a static buffer that is overwritten on each call.
 */
const char *usToUtf8(const ustring *str, size_t *len)
{
    static iconv_t conv = NULL;

    if (conv == NULL) {
        conv = iconv_open("UTF-8", "WCHAR_T");

        dbgAssert(stderr, conv != (iconv_t) -1,
                "Could not create converter from UTF-8 to wchar_t");
    }

    const char *utf8_buf =
        convert_charset(conv, (char *) usGet(str), 4 * usLen(str), len);

    return utf8_buf;
}

/*
 * Get a pointer to the data from <str>. Find the size of the returned data
 * using usLen(). Note that this returns a direct pointer to the data in
 * <str>. You are not supposed to change it.
 */
const wchar_t *usGet(const ustring *str)
{
    return str->data ? str->data : L"";
}

/*
 * Reset <str> to an empty state. Does not free its internal data (use
 * usClear() for that).
 */
ustring *usRewind(ustring *str)
{
    if (str->data != NULL) str->data[0] = 0;

    str->used = 0;

    return str;
}

/*
 * Get the length of <str>.
 */
int usLen(const ustring *str)
{
    return str->used;
}

/*
 * Return TRUE if <str> is empty, otherwise FALSE.
 */
int usIsEmpty(const ustring *str)
{
    return str->used == 0;
}

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
ustring *usCat(ustring *base, const ustring *addition)
{
    usAdd(base, usGet(addition), usLen(addition));

    return base;
}

/*
 * Strip <left> bytes from the left and <right> bytes from the right of <str>.
 */
ustring *usStrip(ustring *str, size_t left, size_t right)
{
    if (str->data == NULL) return str;

    if (left  > str->used) left = str->used;
    if (right > str->used - left) right = str->used - left;

    memmove(str->data, str->data + left,
            (str->used - left - right) * sizeof(wchar_t));

    str->used -= (left + right);

    *(str->data + str->used) = '\0';

    return str;
}

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right>, either in size, or (when both have the same size) according to
 * memcmp().
 */
int usCompare(const ustring *left, const ustring *right)
{
    int len1 = usLen(left);
    int len2 = usLen(right);

    if (len1 < len2)
        return -1;
    else if (len2 > len1)
        return 1;
    else
        return memcmp(usGet(left), usGet(right), len1);
}

/*
 * Return true if <str> starts with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
bool usStartsWith(const ustring *str, const wchar_t *fmt, ...)
{
    va_list ap, ap_copy;
    int r, size = 0;
    wchar_t *pat = NULL;

    va_start(ap, fmt);

    do {
        size = (size == 0) ? 32 : 2 * size;

        pat = realloc(pat, size * sizeof(wchar_t));

        va_copy(ap_copy, ap);
        r = vswprintf(pat, size, fmt, ap_copy);
        va_end(ap_copy);
    } while (r == -1);

    va_end(ap);

    r = wcsncmp(usGet(str), pat, wcslen(pat));

    free(pat);

    return r == 0;
}

/*
 * Return true if <str> ends with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
bool usEndsWith(const ustring *str, const wchar_t *fmt, ...)
{
    va_list ap, ap_copy;
    int r, size = 0;
    wchar_t *pat = NULL;

    va_start(ap, fmt);

    do {
        size = (size == 0) ? 32 : 2 * size;

        pat = realloc(pat, size * sizeof(wchar_t));

        va_copy(ap_copy, ap);
        r = vswprintf(pat, size, fmt, ap_copy);
        va_end(ap_copy);
    } while (r == -1);

    va_end(ap);

    r = wcscmp(usGet(str) + usLen(str) - wcslen(pat), pat);

    free(pat);

    return r == 0;
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(void)
{
    ustring str1 = { 0 };
    ustring str2 = { 0 };
    ustring *str3;

    // ** usRewind

    usRewind(&str1);

    make_sure_that(usLen(&str1) == 0);
    make_sure_that(usIsEmpty(&str1));

    // ** The usAdd* family

    usAdd(&str1, L"ABCDEF", 3);

    make_sure_that(usLen(&str1) == 3);
    make_sure_that(!usIsEmpty(&str1));
    make_sure_that(wcscmp(usGet(&str1), L"ABC") == 0);

    // Add a single wide character...

    usAddC(&str1, L'D');

    make_sure_that(usLen(&str1) == 4);
    make_sure_that(wcscmp(usGet(&str1), L"ABCD") == 0);

    // Add a formatted number...

    usAddF(&str1, L"%d", 12);

    make_sure_that(usLen(&str1) == 6);
    make_sure_that(wcscmp(usGet(&str1), L"ABCD12") == 0);

    // Add a formatted ASCII string...

    usAddF(&str1, L"%s", "3");

    make_sure_that(usLen(&str1) == 7);
    make_sure_that(wcscmp(usGet(&str1), L"ABCD123") == 0);

    // Add a formatted wide string...

    usAddF(&str1, L"%ls", L"4");

    make_sure_that(usLen(&str1) == 8);
    make_sure_that(wcscmp(usGet(&str1), L"ABCD1234") == 0);

    // Add a null-terminated wide string...

    usAddS(&str1, L"XYZ");

    make_sure_that(usLen(&str1) == 11);
    make_sure_that(wcscmp(usGet(&str1), L"ABCD1234XYZ") == 0);

    // ** Overflow the initial 16 allocated bytes.

    usAddF(&str1, L"%ls", L"1234567890");

    make_sure_that(usLen(&str1) == 21);
    make_sure_that(wcscmp(usGet(&str1), L"ABCD1234XYZ1234567890") == 0);

    // ** The usSet* family

    usSet(&str1, L"ABCDEF", 3);

    make_sure_that(usLen(&str1) == 3);
    make_sure_that(wcscmp(usGet(&str1), L"ABC") == 0);

    usSetC(&str1, L'D');

    make_sure_that(usLen(&str1) == 1);
    make_sure_that(wcscmp(usGet(&str1), L"D") == 0);

    usSetF(&str1, L"%d", 1234);

    make_sure_that(usLen(&str1) == 4);
    make_sure_that(wcscmp(usGet(&str1), L"1234") == 0);

    usSetS(&str1, L"ABCDEF");

    make_sure_that(usLen(&str1) == 6);
    make_sure_that(wcscmp(usGet(&str1), L"ABCDEF") == 0);

    // ** usRewind again

    usRewind(&str1);

    make_sure_that(usLen(&str1) == 0);
    make_sure_that(wcscmp(usGet(&str1), L"") == 0);

    // ** usCat

    usSet(&str1, L"ABC", 3);
    usSet(&str2, L"DEF", 3);

    str3 = usCat(&str1, &str2);

    make_sure_that(&str1 == str3);

    make_sure_that(usLen(&str1) == 6);
    make_sure_that(wcscmp(usGet(&str1), L"ABCDEF") == 0);

    make_sure_that(usLen(&str2) == 3);
    make_sure_that(wcscmp(usGet(&str2), L"DEF") == 0);

    // ** usFinish

    wchar_t *r;

    // Regular string
    str3 = usSetS(usCreate(), L"ABCDEF");
    make_sure_that(wcscmp((r = usFinish(str3)), L"ABCDEF") == 0);
    free(r);

    // Empty ustring (where str->data == NULL)
    str3 = usCreate();
    make_sure_that(wcscmp((r = usFinish(str3)), L"") == 0);
    free(r);

    // Reset ustring (where str->data != NULL)
    str3 = usSetS(usCreate(), L"ABCDEF");
    usRewind(str3);
    make_sure_that(wcscmp((r = usFinish(str3)), L"") == 0);
    free(r);

    // ** usStrip

    usSetF(&str1, L"ABCDEF");

    make_sure_that(wcscmp(usGet(usStrip(&str1, 0, 0)), L"ABCDEF") == 0);
    make_sure_that(wcscmp(usGet(usStrip(&str1, 1, 0)), L"BCDEF") == 0);
    make_sure_that(wcscmp(usGet(usStrip(&str1, 0, 1)), L"BCDE") == 0);
    make_sure_that(wcscmp(usGet(usStrip(&str1, 1, 1)), L"CD") == 0);
    make_sure_that(wcscmp(usGet(usStrip(&str1, 3, 3)), L"") == 0);

    // ** usStartsWith, usEndsWith

    usSetS(&str1, L"abcdef");

    make_sure_that(usStartsWith(&str1, L"abc") == true);
    make_sure_that(usStartsWith(&str1, L"def") == false);
    make_sure_that(usEndsWith(&str1, L"def") == true);
    make_sure_that(usEndsWith(&str1, L"abc") == false);

    make_sure_that(usStartsWith(&str1, L"%ls", L"abc") == true);
    make_sure_that(usStartsWith(&str1, L"%ls", L"def") == false);
    make_sure_that(usEndsWith(&str1, L"%ls", L"def") == true);
    make_sure_that(usEndsWith(&str1, L"%ls", L"abc") == false);

    make_sure_that(usStartsWith(&str1, L"%s", "abc") == true);
    make_sure_that(usStartsWith(&str1, L"%s", "def") == false);
    make_sure_that(usEndsWith(&str1, L"%s", "def") == true);
    make_sure_that(usEndsWith(&str1, L"%s", "abc") == false);

    usClear(&str1);

    usSetS(&str1, L"123456789");

    make_sure_that(usStartsWith(&str1, L"123") == true);
    make_sure_that(usStartsWith(&str1, L"789") == false);
    make_sure_that(usEndsWith(&str1, L"789") == true);
    make_sure_that(usEndsWith(&str1, L"123") == false);

    make_sure_that(usStartsWith(&str1, L"%d", 123) == true);
    make_sure_that(usStartsWith(&str1, L"%d", 789) == false);
    make_sure_that(usEndsWith(&str1, L"%d", 789) == true);
    make_sure_that(usEndsWith(&str1, L"%d", 123) == false);

    // ** usSetT and usAddT.

    usSetT(&str1, 1660842836, "CET", L"%Y-%m-%d");

    make_sure_that(usLen(&str1) == 10);
    make_sure_that(wcscmp(usGet(&str1), L"2022-08-18") == 0);

    usAddT(&str1, 1660842836, "CET", L" %H:%M:%S");

    make_sure_that(usLen(&str1) == 19);
    make_sure_that(wcscmp(usGet(&str1), L"2022-08-18 19:13:56") == 0);

    usSetT(&str1, 1660842836, "UTC", L"%Y-%m-%d");

    make_sure_that(usLen(&str1) == 10);
    make_sure_that(wcscmp(usGet(&str1), L"2022-08-18") == 0);

    usAddT(&str1, 1660842836, "UTC", L" %H:%M:%S");

    make_sure_that(usLen(&str1) == 19);
    make_sure_that(wcscmp(usGet(&str1), L"2022-08-18 17:13:56") == 0);

    usClear(&str1);

    const char *utf8_txt = "αß¢";
    size_t utf8_len = strlen(utf8_txt);

    usFromUtf8(&str1, utf8_txt, utf8_len);

    make_sure_that(wcscmp(usGet(&str1), L"αß¢") == 0);

    usSetS(&str1, L"Smørrebrød i københavn");

    utf8_txt = usToUtf8(&str1, &utf8_len);

    make_sure_that(strcmp(utf8_txt, "Smørrebrød i københavn") == 0);

    return errors;
}
#endif
