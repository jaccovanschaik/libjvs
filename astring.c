/*
 * astring.c: Provides ASCII strings.
 *
 * astring.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: astring.c 500 2025-06-02 10:01:49Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

#include "astring.h"

#define INITIAL_SIZE 16

/*
 * Increase the size of <str> to allow for another <len> bytes.
 */
static void as_increase_by(astring *str, size_t len)
{
    size_t new_len;

    while (str->used + len + 1 > str->size) {
        if (str->size == 0)
            new_len = INITIAL_SIZE;
        else
            new_len = 2 * str->size;

        str->data = realloc(str->data, new_len);

        str->size = new_len;
    }
}

/*
 * Create an astring and initialize it using the given printf-compatible <fmt>
 * string and subsequent parameters. <fmt> may be NULL, in which case the
 * string remains empty.
 */
__attribute__((format (printf, 1, 2)))
astring *asCreate(const char *fmt, ...)
{
    astring *str = calloc(1, sizeof(*str));

    if (fmt) {
        va_list ap;

        va_start(ap, fmt);
        asSetV(str, fmt, ap);
        va_end(ap);
    }

    return str;
}

/*
 * Initialize astring <str> using the given printf-compatible <fmt> string and
 * subsequent parameters. <fmt> may be NULL, in which case the string is
 * cleared. This function assumes the given string has not been initialized
 * and may contain garbage. It therefore will not discard any old content, if
 * it should have any. To set the value of an astring that *has* been
 * initialized, simply use one of the asSet functions.
 */
__attribute__((format (printf, 2, 3)))
astring *asInit(astring *str, const char *fmt, ...)
{
    str->size = INITIAL_SIZE;
    str->data = calloc(1, str->size);
    str->used = 0;

    if (fmt) {
        va_list ap;

        va_start(ap, fmt);
        asSetV(str, fmt, ap);
        va_end(ap);
    }

    return str;
}

/*
 * Clear <str>, freeing its internal data. Use this if you have an automatically
 * allocated astring and want to completely discard its contents before it goes
 * out of scope. If <str> was dynamically allocated (using asCreate() above)
 * you may free() it after calling this function (or you could simply call
 * asDestroy()).
 */
void asClear(astring *str)
{
    free(str->data);

    memset(str, 0, sizeof(astring));
}

/*
 * Detach the character string inside <str> and return it, and reinitialize
 * the astring. The caller is responsible for the returned data after this and
 * should free() it when finished.
 */
char *asDetach(astring *str)
{
    char *data = str->data;

    str->data = NULL;
    str->size = 0;
    str->used = 0;

    return data ? data : strdup("");
}

/*
 * Destroy <str>, but save and return its contents. The caller is responsible
 * for the returned data after this and should free() it when finished.
 */
char *asFinish(astring *str)
{
    char *data = str->data;

    free(str);

    return data ? data : strdup("");
}

/*
 * Destroy <str>, together with the data it contains.
 */
void asDestroy(astring *str)
{
    free(str->data);
    free(str);
}

/*
 * Add <len> bytes, starting at <data> to <str>.
 */
astring *asAdd(astring *str, const void *data, size_t len)
{
    as_increase_by(str, len);

    memcpy(str->data + str->used, data, len);

    str->used += len;

    str->data[str->used] = '\0';

    return str;
}

/*
 * Add the single character <c> to <str>.
 */
astring *asAddC(astring *str, char c)
{
    asAdd(str, &c, 1);

    return str;
}

/*
 * Append a string to <str>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
astring *asAddV(astring *str, const char *fmt, va_list ap)
{
    va_list my_ap;

    while (true) {
        size_t req_size, avail_size = str->size - str->used;

        va_copy(my_ap, ap);
        req_size = vsnprintf(str->data + str->used, avail_size, fmt, my_ap);
        va_end(my_ap);

        if (req_size < avail_size) {
            str->used += req_size;
            break;
        }

        as_increase_by(str, req_size);
    }

    return str;
}

/*
 * Append a string to <str>, formatted according to <fmt> and the subsequent
 * parameters. */
__attribute__((format (printf, 2, 3)))
astring *asAddF(astring *str, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    asAddV(str, fmt, ap);
    va_end(ap);

    return str;
}

/*
 * Append the null-terminated string <s> to <str>.
 */
astring *asAddS(astring *str, const char *s)
{
    return asAdd(str, s, strlen(s));
}

/*
 * Append a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>. If
 * <tz> is NULL, the time zone in the environment variable TZ is used.
 */
__attribute__((format (strftime, 4, 0)))
astring *asAddT(astring *str, time_t t, const char *tz, const char *fmt)
{
    const char *old_tz = NULL;

    if (tz != NULL) {
        old_tz = getenv("TZ");
        setenv("TZ", tz, 1);
    }

    char *new_fmt = calloc(strlen(fmt) + 2, 1);

    new_fmt[0] = 'x';
    strcat(new_fmt, fmt);

    size_t buf_size = 32;
    char *buf = calloc(1, buf_size);

    int r;

    struct tm *tm = localtime(&t);

    while ((r = strftime(buf, buf_size, new_fmt, tm)) == 0) {
        buf_size *= 2;

        buf = realloc(buf, buf_size);
    }

    if (old_tz != NULL) setenv("TZ", old_tz, 1);

    asAdd(str, buf + 1, strlen(buf) - 1);

    free(new_fmt);
    free(buf);

    return str;
}

/*
 * Replace <str> with the <len> bytes starting at <data>.
 */
astring *asSet(astring *str, const void *data, size_t len)
{
    asRewind(str);

    return asAdd(str, data, len);
}

/*
 * Set <str> to the single character <c>.
 */
astring *asSetC(astring *str, char c)
{
    asRewind(str);

    return asAdd(str, &c, 1);
}

/*
 * Set <str> to a string formatted according to <fmt> and the subsequent
 * parameters.
 */
__attribute__((format (printf, 2, 3)))
astring *asSetF(astring *str, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    asSetV(str, fmt, ap);
    va_end(ap);

    return str;
}

/*
 * Replace <str> with a string formatted according to <fmt> and the subsequent
 * parameters contained in <ap>.
 */
astring *asSetV(astring *str, const char *fmt, va_list ap)
{
    asRewind(str);

    return asAddV(str, fmt, ap);
}

/*
 * Set <str> to the null-terminated string <str>.
 */
astring *asSetS(astring *str, const char *s)
{
    return asSet(str, s, strlen(s));
}

/*
 * Write a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>.
 * If <tz> is NULL, the time zone in the environment variable TZ is used.
 */
__attribute__((format (strftime, 4, 0)))
astring *asSetT(astring *str, time_t t, const char *tz, const char *fmt)
{
    asRewind(str);

    return asAddT(str, t, tz, fmt);
}

/*
 * Get a pointer to the data from <str>. Find the size of the returned data
 * using asLen(). Note that this returns a direct pointer to the data in
 * <str>. You are not supposed to change it.
 */
const char *asGet(const astring *str)
{
    return str->data ? str->data : "";
}

/*
 * Reset <str> to an empty state. Does not free its internal data (use
 * asClear() for that).
 */
astring *asRewind(astring *str)
{
    if (str->data != NULL) str->data[0] = 0;

    str->used = 0;

    return str;
}

/*
 * Get the length of <str>.
 */
int asLen(const astring *str)
{
    return str->used;
}

/*
 * Return TRUE if <str> is empty, otherwise FALSE.
 */
int asIsEmpty(const astring *str)
{
    return str->used == 0;
}

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
astring *asCat(astring *base, const astring *addition)
{
    asAdd(base, asGet(addition), asLen(addition));

    return base;
}

/*
 * Strip <left> bytes from the left and <right> bytes from the right of <str>.
 */
astring *asStrip(astring *str, size_t left, size_t right)
{
    if (str->data == NULL) return str;

    if (left  > str->used) left = str->used;
    if (right > str->used - left) right = str->used - left;

    memmove(str->data, str->data + left, str->used - left - right);

    str->used -= (left + right);

    *(str->data + str->used) = '\0';

    return str;
}

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right>, either in size, or (when both have the same size) according to
 * memcmp().
 */
int asCompare(const astring *left, const astring *right)
{
    int len1 = asLen(left);
    int len2 = asLen(right);

    if (len1 < len2)
        return -1;
    else if (len2 > len1)
        return 1;
    else
        return memcmp(asGet(left), asGet(right), len1);
}

/*
 * Return true if <str> starts with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool asStartsWith(const astring *str, const char *fmt, ...)
{
    va_list ap;
    char *pat;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&pat, fmt, ap);
    va_end(ap);

    r = strncmp(asGet(str), pat, strlen(pat));

    free(pat);

    return r == 0;
}

/*
 * Return true if <str> ends with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool asEndsWith(const astring *str, const char *fmt, ...)
{
    va_list ap;
    char *pat;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&pat, fmt, ap);
    va_end(ap);

    r = strcmp(asGet(str) + asLen(str) - strlen(pat), pat);

    free(pat);

    return r == 0;
}

#ifdef TEST
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
#endif
