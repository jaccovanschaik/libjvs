#ifndef LIBJVS_STRING_H
#define LIBJVS_STRING_H

/*
 * astring.h: provides ASCII strings.
 *
 * astring.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: astring.h 467 2022-11-20 00:05:38Z jacco $
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
    char  *data;    // Pointer to astring data.
    size_t used;    // Bytes in use (excluding a trailing null byte).
    size_t size;    // Bytes allocated.
} astring;

/*
 * Create an empty astring.
 */
astring *asCreate(void);

/*
 * Initialize astring <str>.
 */
astring *asInit(astring *str);

/*
 * Clear <str>, freeing its internal data. Use this if you have an automatically
 * allocated astring and want to completely discard its contents before it goes
 * out of scope. If <str> was dynamically allocated (using asCreate() above)
 * you may free() it after calling this function (or you could simply call
 * asDestroy()).
 */
void asClear(astring *str);

/*
 * Detach the character string inside <str> and return it, and reinitialize
 * the astring. The caller is responsible for the returned data after this and
 * should free() it when finished.
 */
char *asDetach(astring *str);

/*
 * Destroy <str>, but save and return its contents. The caller is responsible
 * for the returned data after this and should free() it when finished.
 */
char *asFinish(astring *str);

/*
 * Destroy <str>, together with the data it contains.
 */
void asDestroy(astring *str);

/*
 * Add <len> bytes, starting at <data> to <str>.
 */
astring *asAdd(astring *str, const void *data, size_t len);

/*
 * Add the single character <c> to <str>.
 */
astring *asAddC(astring *str, char c);

/*
 * Append a string to <str>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
astring *asAddV(astring *str, const char *fmt, va_list ap);

/*
 * Append a string to <str>, formatted according to <fmt> and the subsequent
 * parameters. */
__attribute__((format (printf, 2, 3)))
astring *asAddF(astring *str, const char *fmt, ...);

/*
 * Append the null-terminated string <s> to <str>.
 */
astring *asAddS(astring *str, const char *s);

/*
 * Append a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>. If
 * <tz> is NULL, the time zone in the environment variable TZ is used.
 */
__attribute__((format (strftime, 4, 0)))
astring *asAddT(astring *str, time_t t, const char *tz, const char *fmt);

/*
 * Replace <str> with the <len> bytes starting at <data>.
 */
astring *asSet(astring *str, const void *data, size_t len);

/*
 * Set <str> to the single character <c>.
 */
astring *asSetC(astring *str, char c);

/*
 * Set <str> to a string formatted according to <fmt> and the subsequent
 * parameters.
 */
__attribute__((format (printf, 2, 3)))
astring *asSetF(astring *str, const char *fmt, ...);

/*
 * Replace <str> with a string formatted according to <fmt> and the subsequent
 * parameters contained in <ap>.
 */
astring *asSetV(astring *str, const char *fmt, va_list ap);

/*
 * Set <str> to the null-terminated string <str>.
 */
astring *asSetS(astring *str, const char *s);

/*
 * Write a formatted time to <str>, based on the UNIX timestamp in <t>, the
 * timezone in <tz> and the strftime-compatible format string in <fmt>.
 * If <tz> is NULL, the time zone in the environment variable TZ is used.
 */
__attribute__((format (strftime, 4, 0)))
astring *asSetT(astring *str, time_t t, const char *tz, const char *fmt);

/*
 * Get a pointer to the data from <str>. Find the size of the returned data
 * using asLen(). Note that this returns a direct pointer to the data in
 * <str>. You are not supposed to change it.
 */
const char *asGet(const astring *str);

/*
 * Reset <str> to an empty state. Does not free its internal data (use
 * asClear() for that).
 */
astring *asRewind(astring *str);

/*
 * Get the length of <str>.
 */
int asLen(const astring *str);

/*
 * Return TRUE if <str> is empty, otherwise FALSE.
 */
int asIsEmpty(const astring *str);

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
astring *asCat(astring *base, const astring *addition);

/*
 * Strip <left> bytes from the left and <right> bytes from the right of <str>.
 */
astring *asStrip(astring *str, size_t left, size_t right);

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right>, either in size, or (when both have the same size) according to
 * memcmp().
 */
int asCompare(const astring *left, const astring *right);

/*
 * Return true if <str> starts with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool asStartsWith(const astring *str, const char *fmt, ...);

/*
 * Return true if <str> ends with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool asEndsWith(const astring *str, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
