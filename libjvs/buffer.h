/*
 * Provides growing byte buffers. Strings can contain binary data, but are
 * always null-terminated for convenience's sake.
 *
 * Copyright:	(c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: string.h 281 2012-04-12 19:30:39Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef JVS_STRING_H
#define JVS_STRING_H

#include <stdarg.h>

typedef struct {
    char *data;
    int   size;   /* The number of bytes allocated. */
    int   used;   /* The number of bytes in use (incl. a trailing null byte). */
} Buffer;

/*
 * Encode buffer <value> into <buf>.
 */
int bufEncode(Buffer *buf, const Buffer *value);

/*
 * Get a binary-encoded buffer from <buf> and store it in <value>. If
 * succesful, the first part where the int8_t was stored will be
 * stripped from <buf>.
 */
int bufDecode(Buffer *buf, Buffer *value);

/*
 * Get an encoded buffer from <ptr> (with <remaining> bytes
 * remaining) and store it in <value>.
 */
int bufExtract(const char **ptr, int *remaining, Buffer *value);

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right> (according to bufcmp()).
 */
int bufCompare(const Buffer *left, const Buffer *right);

/*
 * Create an empty buffer.
 */
Buffer *bufCreate(void);

/*
 * Initialize buffer <buf>.
 */
Buffer *bufInit(Buffer *buf);

/*
 * Destroy <buf>, together with the data it contains.
 */
void bufDestroy(Buffer *buf);

/*
 * Destroy <buf>, but save and return its contents. The caller is responsible
 * for the contents after this and should free() them when finished.
 */
char *bufFinish(Buffer *buf);

/*
 * Add <len> bytes, starting at <data> to <buf>.
 */
Buffer *bufAdd(Buffer *buf, const void *data, int len);

/* Add the single character <c>. */
Buffer *bufAddC(Buffer *buf, char c);

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters.
 */
Buffer *bufAddF(Buffer *buf, const char *fmt, ...);

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
Buffer *bufAddV(Buffer *buf, const char *fmt, va_list ap);

/*
 * Replace <buf> with the <len> bytes starting at <data>.
 */
Buffer *bufSet(Buffer *buf, const void *data, int len);

/* Replace <buf> with the single character <c>. */
Buffer *bufSetC(Buffer *buf, char c);

/*
 * Replace <buf> with a string formatted according to <fmt> and with the
 * subsequent parameters.
 */
Buffer *bufSetF(Buffer *buf, const char *fmt, ...);

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
Buffer *bufTrim(Buffer *buf, unsigned int left, unsigned int right);

#endif
