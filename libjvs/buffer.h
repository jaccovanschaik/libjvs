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
    int   act_len;  /* The number of bytes in use (excluding a trailing null byte!). */
    int   max_len;  /* The number of bytes allocated. */
} Buffer;

enum {
    PACK_UINT8,
    PACK_UINT16,
    PACK_UINT32,
    PACK_UINT64,
    PACK_FLOAT,
    PACK_DOUBLE,
    PACK_TEXT,
    PACK_STRING,
    PACK_BUFFER
};

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
Buffer *bufAdd(Buffer *buf, const void *data, int len);

/* Add the single character <c>. */
Buffer *bufAddC(Buffer *buf, char c);

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters.
 */
Buffer *bufAddF(Buffer *buf, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

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
Buffer *bufTrim(Buffer *buf, unsigned int left, unsigned int right);

/*
 * Pack <buf> according to the arguments that follow, *without* clearing
 * the buffer first. The argument list consists of type/value pairs,
 * followed by END. The following types are allowed, accompanied by
 * values of the given type.
 *
 *  Descriptor  Argument type   Value added to buffer
 *  ----------  -------------   ---------------------
 *  PACK_UINT8  uint32_t        unsigned 8-bit int
 *  PACK_UINT16 uint32_t        unsigned 16-bit big-endian int
 *  PACK_UINT32 uint32_t        unsigned 32-bit big-endian int
 *  PACK_UINT64 uint64_t        unsigned 64-bit big-endian int
 *  PACK_FLOAT  double          IEEE-754 32-bit big-endian floating point
 *  PACK_DOUBLE double          IEEE-754 64-bit big-endian floating point
 *  PACK_STRING char *          unsigned 32-bit big-endian length as reported by strlen(),
 *                              followed by the contents of the string.
 *  PACK_BUFFER Buffer *        unsigned 32-bit big-endian length as reported by bufLen(),
 *                              followed by the contents of the buffer.
 */
Buffer *bufPack(Buffer *buf, ...);

/*
 * Unpack <buf> into to the arguments that follow. The argument list
 * consists of type/value pairs, followed by END. The following types
 * are allowed, accompanied by values of the given type.
 *
 *  Descriptor  Argument type   Value read from buffer
 *  ----------  -------------   ---------------------
 *  PACK_UINT8  uint32_t *      unsigned 8-bit int
 *  PACK_UINT16 uint32_t *      unsigned 16-bit big-endian int
 *  PACK_UINT32 uint32_t *      unsigned 32-bit big-endian int
 *  PACK_UINT64 uint64_t *      unsigned 64-bit big-endian int
 *  PACK_FLOAT  double *        IEEE-754 32-bit big-endian floating point
 *  PACK_DOUBLE double *        IEEE-754 64-bit big-endian floating point
 *  PACK_STRING char **         unsigned 32-bit big-endian length,
 *                              followed by the contents of the string.
 *  PACK_BUFFER Buffer *        unsigned 32-bit big-endian length,
 *                              followed by the contents of the buffer.
 */
Buffer *bufUnpack(Buffer *buf, ...);

#endif
