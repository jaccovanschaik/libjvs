/*
 * Provides growing byte buffers. Strings can contain binary data, but are
 * always null-terminated for convenience's sake.
 *
 * Copyright:	(c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef JVS_STRING_H
#define JVS_STRING_H

#include <stdarg.h>

typedef struct {
    char  *data;
    size_t act_len; /* The number of bytes in use (excluding a trailing null byte!). */
    size_t max_len; /* The number of bytes allocated. */
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
 * Reset buffer <buf> to a virgin state, freeing its internal data. Use this if you have an
 * automatically allocated Buffer and want to completely discard its contents.
 */
void bufReset(Buffer *buf);

/*
 * Detach the contents of <buf>. The buffer is re-initialized and the
 * old contents are returned. The caller is responsible for the contents
 * after this and should free() them when finished.
 */
char *bufDetach(Buffer *buf);

/*
 * Destroy <buf>, but save and return its contents. The caller is responsible
 * for the contents after this and should free() them when finished.
 */
char *bufFinish(Buffer *buf);

/*
 * Destroy <buf>, together with the data it contains.
 */
void bufDestroy(Buffer *buf);

/*
 * Add <len> bytes, starting at <data> to <buf>.
 */
Buffer *bufAdd(Buffer *buf, const void *data, size_t len);

/*
 * Add the single character <c>.
 */
Buffer *bufAddC(Buffer *buf, char c);

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
Buffer *bufAddV(Buffer *buf, const char *fmt, va_list ap);

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters.
 */
Buffer *bufAddF(Buffer *buf, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

/*
 * Replace <buf> with the <len> bytes starting at <data>.
 */
Buffer *bufSet(Buffer *buf, const void *data, size_t len);

/*
 * Set <buf> to the single character <c>.
 */
Buffer *bufSetC(Buffer *buf, char c);

/*
 * Set <buf> to a string formatted according to <fmt> and with the
 * subsequent parameters.
 */
Buffer *bufSetF(Buffer *buf, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

/*
 * Replace <buf> with a string formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
Buffer *bufSetV(Buffer *buf, const char *fmt, va_list ap);

/*
 * Get a pointer to the data from <buf>. Find the size of the buffer using
 * bufLen().
 */
const char *bufGet(const Buffer *buf);

/*
 * Clear the data in <buf>.
 */
Buffer *bufClear(Buffer *buf);

/*
 * Get the number of valid bytes in <buf>.
 */
int bufLen(const Buffer *buf);

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
Buffer *bufCat(Buffer *base, const Buffer *addition);

/*
 * Trim <left> bytes from the left and <right> bytes from the right of <buf>.
 */
Buffer *bufTrim(Buffer *buf, size_t left, size_t right);

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to <right>, either in size, or
 * (when both have the same size) according to memcmp().
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
 * This function does the same as strunpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufVaUnpack(Buffer *buf, va_list ap);

/*
 * This function does the same as vstrunpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufUnpack(Buffer *buf, ...);

/*
 * This function assists in building textual lists of the form "Tom, Dick and Harry". Call it three
 * times with the arguments "Tom", "Dick" and "Harry". Set sep1 to ", " and sep2 to " and ". Set
 * is_first to TRUE when passing in "Tom", set is_last to TRUE when passing in "Harry", set them
 * both to FALSE for "Dick". Returns the same pointer to <buf> that was passed in.
 */
Buffer *bufList(Buffer *buf, const char *sep1, const char *sep2,
        int is_first, int is_last, const char *fmt, ...) __attribute__ ((format (printf, 6, 7)));

#endif
