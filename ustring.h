#ifndef LIBJVS_STRING_H
#define LIBJVS_STRING_H

/*
 * ustring.h: handle Unicode strings.
 *
 * ustring.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: ustring.h 461 2022-08-19 06:10:43Z jacco $
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
} ustring;

/*
 * Create an empty ustring.
 */
ustring *usCreate(void);

/*
 * Initialize ustring <str>.
 */
ustring *usInit(ustring *str);

/*
 * Clear <str>, freeing its internal data. Use this if you have an automatically
 * allocated ustring and want to completely discard its contents before it goes
 * out of scope. If <str> was dynamically allocated (using usCreate() above)
 * you may free() it after calling this function (or you could simply call
 * usDestroy()).
 */
void usClear(ustring *str);

/*
 * Detach the character string inside <str> and return it, and reinitialize
 * the ustring. The caller is responsible for the returned data after this and
 * should free() it when finished.
 */
wchar_t *usDetach(ustring *str);

/*
 * Destroy <str>, but save and return its contents. The caller is responsible
 * for the returned data after this and should free() it when finished.
 */
wchar_t *usFinish(ustring *str);

/*
 * Destroy <str>, together with the data it contains.
 */
void usDestroy(ustring *str);

/*
 * Add <len> bytes, starting at <data> to <str>.
 */
ustring *usAdd(ustring *str, const void *data, size_t len);

/*
 * Add the single character <c> to <str>.
 */
ustring *usAddC(ustring *str, wchar_t c);

/*
 * Append a string to <str>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
ustring *usAddV(ustring *str, const wchar_t *fmt, va_list ap);

/*
 * Append a string to <str>, formatted according to <fmt> and the subsequent
 * parameters. */
ustring *usAddF(ustring *str, const wchar_t *fmt, ...);

/*
 * Append the null-terminated string <s> to <str>.
 */
ustring *usAddS(ustring *str, const wchar_t *s);

/*
 * Append a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>. If
 * <tz> is NULL, the time zone in the environment variable TZ is used.
 */
ustring *usAddT(ustring *str, time_t t, const char *tz, const wchar_t *fmt);

/*
 * Replace <str> with the <len> bytes starting at <data>.
 */
ustring *usSet(ustring *str, const void *data, size_t len);

/*
 * Set <str> to the single character <c>.
 */
ustring *usSetC(ustring *str, wchar_t c);

/*
 * Set <str> to a string formatted according to <fmt> and the subsequent
 * parameters.
 */
ustring *usSetF(ustring *str, const wchar_t *fmt, ...);

/*
 * Replace <str> with a string formatted according to <fmt> and the subsequent
 * parameters contained in <ap>.
 */
ustring *usSetV(ustring *str, const wchar_t *fmt, va_list ap);

/*
 * Set <str> to the null-terminated string <str>.
 */
ustring *usSetS(ustring *str, const wchar_t *s);

/*
 * Write a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>.
 * If <tz> is NULL, the time zone in the environment variable TZ is used.
 */
ustring *usSetT(ustring *str, time_t t, const char *tz, const wchar_t *fmt);

/*
 * Fill <str> using the UTF-8 text in <utf8_text>, which is <utf8_len> bytes
 * (not necessarily characters!) long. Returns the same <str> that was passed
 * in.
 */
ustring *usFromUtf8(ustring *str, const char *utf8_txt, size_t utf8_len);

/*
 * Return a UTF-8 version of <str>. The number of bytes (not necessarily
 * characters!) in the UTF-8 string is returned through <len>. The returned
 * pointer points to a static buffer that is overwritten on each call.
 */
const char *usToUtf8(const ustring *str, size_t *len);

/*
 * Get a pointer to the data from <str>. Find the size of the returned data
 * using usLen(). Note that this returns a direct pointer to the data in
 * <str>. You are not supposed to change it.
 */
const wchar_t *usGet(const ustring *str);

/*
 * Reset <str> to an empty state. Does not free its internal data (use
 * usClear() for that).
 */
ustring *usRewind(ustring *str);

/*
 * Get the length of <str>.
 */
int usLen(const ustring *str);

/*
 * Return TRUE if <str> is empty, otherwise FALSE.
 */
int usIsEmpty(const ustring *str);

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
ustring *usCat(ustring *base, const ustring *addition);

/*
 * Strip <left> bytes from the left and <right> bytes from the right of <str>.
 */
ustring *usStrip(ustring *str, size_t left, size_t right);

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right>, either in size, or (when both have the same size) according to
 * memcmp().
 */
int usCompare(const ustring *left, const ustring *right);

/*
 * Return true if <str> starts with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
bool usStartsWith(const ustring *str, const wchar_t *fmt, ...);

/*
 * Return true if <str> ends with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
bool usEndsWith(const ustring *str, const wchar_t *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
