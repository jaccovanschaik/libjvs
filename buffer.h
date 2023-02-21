#ifndef LIBJVS_STRING_H
#define LIBJVS_STRING_H

/*
 * buffer.h: provides growing byte buffers. Buffers can contain binary data, but
 * are always null-terminated for convenience's sake.
 *
 * buffer.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2023 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: buffer.h 475 2023-02-21 08:08:11Z jacco $
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
    char  *data;    // Pointer to buffer data.
    size_t used;    // Bytes in use (excluding a trailing null byte, so you
                    // have a valid, consistent buffer when all zeroed out).
    size_t size;    // Bytes allocated.
} Buffer;

/*
 * Create an empty buffer.
 */
Buffer *bufCreate(void);

/*
 * Initialize buffer <buf>.
 */
Buffer *bufInit(Buffer *buf);

/*
 * Clear <buf>, freeing its internal data. Use this if you have an automatically
 * allocated Buffer and want to completely discard its contents before it goes
 * out of scope. If <buf> was dynamically allocated (using bufCreate() above)
 * you may free() it after calling this function (or you could simply call
 * bufDestroy()).
 */
void bufClear(Buffer *buf);

/*
 * Detach and return the contents of <buf>, and reinitialize the buffer. The
 * caller is responsible for the returned data after this and should free() it
 * when finished.
 */
char *bufDetach(Buffer *buf);

/*
 * Destroy <buf>, but save and return its contents. If the buffer is empty, an
 * empty string is returned. The caller is responsible for the returned data
 * after this and should free() it when finished.
 */
char *bufFinish(Buffer *buf);

/*
 * Destroy <buf>, but save and return its contents. If the buffer is empty, NULL
 * is returned. The caller is responsible for the returned data after this and
 * should free() it when finished.
 */
char *bufFinishN(Buffer *buf);

/*
 * Destroy <buf>, together with the data it contains.
 */
void bufDestroy(Buffer *buf);

/*
 * Add <len> bytes, starting at <data> to <buf>.
 */
Buffer *bufAdd(Buffer *buf, const void *data, size_t len);

/*
 * Add the single character <c> to <buf>.
 */
Buffer *bufAddC(Buffer *buf, char c);

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
Buffer *bufAddV(Buffer *buf, const char *fmt, va_list ap);

/*
 * Append a string to <buf>, formatted according to <fmt> and the subsequent
 * parameters. */
__attribute__((format (printf, 2, 3)))
Buffer *bufAddF(Buffer *buf, const char *fmt, ...);

/*
 * Append the null-terminated string <str> to <buf>.
 */
Buffer *bufAddS(Buffer *buf, const char *str);

/*
 * Replace <buf> with the <len> bytes starting at <data>.
 */
Buffer *bufSet(Buffer *buf, const void *data, size_t len);

/*
 * Set <buf> to the single character <c>.
 */
Buffer *bufSetC(Buffer *buf, char c);

/*
 * Set <buf> to a string formatted according to <fmt> and the subsequent
 * parameters.
 */
__attribute__((format (printf, 2, 3)))
Buffer *bufSetF(Buffer *buf, const char *fmt, ...);

/*
 * Replace <buf> with a string formatted according to <fmt> and the subsequent
 * parameters contained in <ap>.
 */
Buffer *bufSetV(Buffer *buf, const char *fmt, va_list ap);

/*
 * Set <buf> to the null-terminated string <str>.
 */
Buffer *bufSetS(Buffer *buf, const char *str);

/*
 * Get a pointer to the data from <buf>. Find the size of the returned data
 * using bufLen(). Note that this returns a direct pointer to the data in
 * <buf>. You are not supposed to change it (hence the const keyword).
 */
const char *bufGet(const Buffer *buf);

/*
 * Reset <buf> to an empty state. Does not free its internal data (use
 * bufClear() for that).
 */
Buffer *bufRewind(Buffer *buf);

/*
 * Get the number of valid bytes in <buf>.
 */
int bufLen(const Buffer *buf);

/*
 * Return TRUE if <buf> is empty, otherwise FALSE.
 */
int bufIsEmpty(const Buffer *buf);

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
Buffer *bufCat(Buffer *base, const Buffer *addition);

/*
 * Trim <left> bytes from the left and <right> bytes from the right of <buf>.
 */
Buffer *bufTrim(Buffer *buf, size_t left, size_t right);

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right>, either in size, or (when both have the same size) according to
 * memcmp().
 */
int bufCompare(const Buffer *left, const Buffer *right);

/*
 * This function does the same as vstrpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufVaPack(Buffer *buf, va_list ap);

/*
 * This function does the same as strpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufPack(Buffer *buf, ...);

/*
 * This function does the same as vstrunpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufVaUnpack(Buffer *buf, va_list ap);

/*
 * This function does the same as strunpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufUnpack(Buffer *buf, ...);

/*
 * This function assists in building textual lists of the form "Tom, Dick and
 * Harry". Call it three times with the arguments "Tom", "Dick" and "Harry".
 * Set sep1 to ", " and sep2 to " and ". Set is_first to TRUE when passing in
 * "Tom", set is_last to TRUE when passing in "Harry", set them both to FALSE
 * for "Dick". Returns the same pointer to <buf> that was passed in.
 */
__attribute__((format (printf, 6, 7)))
Buffer *bufList(Buffer *buf, const char *sep1, const char *sep2,
        int is_first, int is_last, const char *fmt, ...);

/*
 * Return true if <buf> starts with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool bufStartsWith(const Buffer *buf, const char *fmt, ...);

/*
 * Return true if <buf> ends with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool bufEndsWith(const Buffer *buf, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
