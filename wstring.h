#ifndef LIBJVS_WSTRING_H
#define LIBJVS_WSTRING_H

/*
 * wstring.h: handle Unicode strings.
 *
 * wstring.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2023 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: wstring.h 478 2023-05-07 08:33:08Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

typedef struct {
    wchar_t *data;  // Pointer to Unicode string data.
    size_t   used;  // Bytes in use (excluding a trailing null byte).
    size_t   size;  // Bytes allocated.
} wstring;

/*
 * Create an empty wstring.
 */
wstring *wsCreate(void);

/*
 * Initialize wstring <str>.
 */
wstring *wsInit(wstring *str);

/*
 * Clear <str>, freeing its internal data. Use this if you have an automatically
 * allocated wstring and want to completely discard its contents before it goes
 * out of scope. If <str> was dynamically allocated (using wsCreate() above)
 * you may free() it after calling this function (or you could simply call
 * wsDestroy()).
 */
void wsClear(wstring *str);

/*
 * Detach the character string inside <str> and return it, and reinitialize
 * the wstring. The caller is responsible for the returned data after this and
 * should free() it when finished.
 */
wchar_t *wsDetach(wstring *str);

/*
 * Destroy <str>, but save and return its contents. The caller is responsible
 * for the returned data after this and should free() it when finished.
 */
wchar_t *wsFinish(wstring *str);

/*
 * Destroy <str>, together with the data it contains.
 */
void wsDestroy(wstring *str);

/*
 * Add <len> bytes, starting at <data> to <str>.
 */
wstring *wsAdd(wstring *str, const void *data, size_t len);

/*
 * Add the single character <c> to <str>.
 */
wstring *wsAddC(wstring *str, wchar_t c);

/*
 * Append a string to <str>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
wstring *wsAddV(wstring *str, const wchar_t *fmt, va_list ap);

/*
 * Append a string to <str>, formatted according to <fmt> and the subsequent
 * parameters. */
wstring *wsAddF(wstring *str, const wchar_t *fmt, ...);

/*
 * Append the null-terminated string <s> to <str>.
 */
wstring *wsAddS(wstring *str, const wchar_t *s);

/*
 * Append a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>. If
 * <tz> is NULL, the time zone in the environment variable TZ is used.
 */
wstring *wsAddT(wstring *str, time_t t, const char *tz, const wchar_t *fmt);

/*
 * Replace <str> with the <len> bytes starting at <data>.
 */
wstring *wsSet(wstring *str, const void *data, size_t len);

/*
 * Set <str> to the single character <c>.
 */
wstring *wsSetC(wstring *str, wchar_t c);

/*
 * Set <str> to a string formatted according to <fmt> and the subsequent
 * parameters.
 */
wstring *wsSetF(wstring *str, const wchar_t *fmt, ...);

/*
 * Replace <str> with a string formatted according to <fmt> and the subsequent
 * parameters contained in <ap>.
 */
wstring *wsSetV(wstring *str, const wchar_t *fmt, va_list ap);

/*
 * Set <str> to the null-terminated string <str>.
 */
wstring *wsSetS(wstring *str, const wchar_t *s);

/*
 * Write a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>.
 * If <tz> is NULL, the time zone in the environment variable TZ is used.
 */
wstring *wsSetT(wstring *str, time_t t, const char *tz, const wchar_t *fmt);

/*
 * Fill <str> using the UTF-8 text in <utf8_text>, which is <utf8_len> bytes
 * (not necessarily characters!) long. Returns the same <str> that was passed
 * in.
 */
wstring *wsFromUtf8(wstring *str, const char *utf8_txt, size_t utf8_len);

/*
 * Return a UTF-8 version of <str>. The number of bytes (not necessarily
 * characters!) in the UTF-8 string is returned through <len>. The returned
 * pointer points to a static buffer that is overwritten on each call.
 */
const char *wsToUtf8(const wstring *str, size_t *len);

/*
 * Get a pointer to the data from <str>. Find the size of the returned data
 * using wsLen(). Note that this returns a direct pointer to the data in
 * <str>. You are not supposed to change it.
 */
const wchar_t *wsGet(const wstring *str);

/*
 * Reset <str> to an empty state. Does not free its internal data (use
 * wsClear() for that).
 */
wstring *wsRewind(wstring *str);

/*
 * Get the length of <str>.
 */
int wsLen(const wstring *str);

/*
 * Return TRUE if <str> is empty, otherwise FALSE.
 */
int wsIsEmpty(const wstring *str);

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
wstring *wsCat(wstring *base, const wstring *addition);

/*
 * Strip <left> bytes from the left and <right> bytes from the right of <str>.
 */
wstring *wsStrip(wstring *str, size_t left, size_t right);

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right>, either in size, or (when both have the same size) according to
 * memcmp().
 */
int wsCompare(const wstring *left, const wstring *right);

/*
 * Return true if <str> starts with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
bool wsStartsWith(const wstring *str, const wchar_t *fmt, ...);

/*
 * Return true if <str> ends with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
bool wsEndsWith(const wstring *str, const wchar_t *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
