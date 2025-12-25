/*
 * buffer.c: Provides growing byte buffers. Buffers always include a trailing
 * null-byte to make it easier to handle them as strings.
 *
 * buffer.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: buffer.c 522 2025-12-25 15:50:26Z jacco $
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

#include "buffer.h"
#include "utils.h"
#include "defs.h"
#include "debug.h"

#define INITIAL_SIZE 16

/*
 * Increase the size of <buf> to allow for another <len> bytes.
 */
static void buf_increase_by(Buffer *buf, size_t len)
{
    size_t new_len;

    while (buf->used + len + 1 > buf->size) {
        if (buf->size == 0)
            new_len = INITIAL_SIZE;
        else
            new_len = 2 * buf->size;

        buf->data = realloc(buf->data, new_len);

        dbgAssert(stderr, buf->data != NULL,
                "Could not (re-)allocate buffer data (%zd bytes requested)",
                new_len);

        buf->size = new_len;
    }
}

/*
 * Create an empty buffer. If <fmt> is not NULL, set the contents of the
 * buffer to a string derived from <fmt> and the parameters in <ap>.
 */
Buffer *bufCreateV(const char *fmt, va_list ap)
{
    Buffer *buf = calloc(1, sizeof(*buf));

    dbgAssert(stderr, buf != NULL, "Could not allocate buffer");

    if (fmt != NULL) {
        bufAddV(buf, fmt, ap);
    }

    return buf;
}

/*
 * Create an empty buffer. If <fmt> is not NULL, set the contents of the
 * buffer to a string derived from <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 1, 2)))
Buffer *bufCreate(const char *fmt, ...)
{
    Buffer *buf;
    va_list ap;

    va_start(ap, fmt);
    buf = bufCreateV(fmt, ap);
    va_end(ap);

    return buf;
}

/*
 * Initialize buffer <buf>.
 */
Buffer *bufInit(Buffer *buf)
{
    buf->size = INITIAL_SIZE;
    buf->data = calloc(1, buf->size);
    buf->used = 0;

    dbgAssert(stderr, buf->data != NULL || buf->size == 0,
            "Could not allocate initial buffer data (%zd bytes requested)",
            buf->size);

    return buf;
}

/*
 * Clear <buf>, freeing its internal data. Use this if you have an automatically
 * allocated Buffer and want to completely discard its contents before it goes
 * out of scope. If <buf> was dynamically allocated (using bufCreate() above)
 * you may free() it after calling this function (or you could simply call
 * bufDestroy()).
 */
void bufClear(Buffer *buf)
{
    free(buf->data);

    memset(buf, 0, sizeof(Buffer));
}

/*
 * Detach and return the contents of <buf>, and reinitialize the buffer. The
 * caller is responsible for the returned data after this and should free() it
 * when finished.
 */
char *bufDetach(Buffer *buf)
{
    char *data = buf->data;

    buf->data = NULL;
    buf->size = 0;
    buf->used = 0;

    return data ? data : strdup("");
}

/*
 * Destroy <buf>, but save and return its contents. If the buffer is empty, an
 * empty string is returned. The caller is responsible for the returned data
 * after this and should free() it when finished.
 */
char *bufFinish(Buffer *buf)
{
    char *data = buf->data;

    free(buf);

    return data ? data : strdup("");
}

/*
 * Destroy <buf>, but save and return its contents. If the buffer is empty, NULL
 * is returned. The caller is responsible for the returned data after this and
 * should free() it when finished.
 */
char *bufFinishN(Buffer *buf)
{
    char *data;

    if (bufIsEmpty(buf)) {
        free(buf->data);
        data = NULL;
    }
    else {
        data = buf->data;
    }

    free(buf);

    return data;
}

/*
 * Destroy <buf>, together with the data it contains.
 */
void bufDestroy(Buffer *buf)
{
    free(buf->data);
    free(buf);
}

/*
 * Add <len> bytes, starting at <data> to <buf>.
 */
Buffer *bufAdd(Buffer *buf, const void *data, size_t len)
{
    buf_increase_by(buf, len);

    memcpy(buf->data + buf->used, data, len);

    buf->used += len;

    buf->data[buf->used] = '\0';

    return buf;
}

/*
 * Add the single character <c> to <buf>.
 */
Buffer *bufAddC(Buffer *buf, char c)
{
    bufAdd(buf, &c, 1);

    return buf;
}

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
Buffer *bufAddV(Buffer *buf, const char *fmt, va_list ap)
{
    va_list my_ap;

    while (true) {
        size_t req_size, avail_size = buf->size - buf->used;

        va_copy(my_ap, ap);
        req_size = vsnprintf(buf->data + buf->used, avail_size, fmt, my_ap);
        va_end(my_ap);

        if (req_size < avail_size) {
            buf->used += req_size;
            break;
        }

        buf_increase_by(buf, req_size);
    }

    return buf;
}

/*
 * Append a string to <buf>, formatted according to <fmt> and the subsequent
 * parameters. */
__attribute__((format (printf, 2, 3)))
Buffer *bufAddF(Buffer *buf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    bufAddV(buf, fmt, ap);
    va_end(ap);

    return buf;
}

/*
 * Append the null-terminated string <str> to <buf>.
 */
Buffer *bufAddS(Buffer *buf, const char *str)
{
    return bufAdd(buf, str, strlen(str));
}

/*
 * Replace <buf> with the <len> bytes starting at <data>.
 */
Buffer *bufSet(Buffer *buf, const void *data, size_t len)
{
    bufRewind(buf);

    return bufAdd(buf, data, len);
}

/*
 * Set <buf> to the single character <c>.
 */
Buffer *bufSetC(Buffer *buf, char c)
{
    bufRewind(buf);

    return bufAdd(buf, &c, 1);
}

/*
 * Set <buf> to a string formatted according to <fmt> and the subsequent
 * parameters.
 */
__attribute__((format (printf, 2, 3)))
Buffer *bufSetF(Buffer *buf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    bufSetV(buf, fmt, ap);
    va_end(ap);

    return buf;
}

/*
 * Replace <buf> with a string formatted according to <fmt> and the subsequent
 * parameters contained in <ap>.
 */
Buffer *bufSetV(Buffer *buf, const char *fmt, va_list ap)
{
    bufRewind(buf);

    return bufAddV(buf, fmt, ap);
}

/*
 * Set <buf> to the null-terminated string <str>.
 */
Buffer *bufSetS(Buffer *buf, const char *str)
{
    return bufSet(buf, str, strlen(str));
}

/*
 * Get a pointer to the data from <buf>. Find the size of the returned data
 * using bufLen(). Note that this returns a direct pointer to the data in
 * <buf>. You are not supposed to change it (hence the const keyword).
 */
const char *bufGet(const Buffer *buf)
{
    return buf->data ? buf->data : "";
}

/*
 * Get the character at position <pos> from <buf>. If that is beyond the end
 * of the buffer, a null byte is returned.
 */
char bufGetC(const Buffer *buf, size_t pos)
{
    if (buf->data && pos < bufLen(buf))
        return buf->data[pos];
    else
        return '\0';
}

/*
 * Reset <buf> to an empty state. Does not free its internal data (use
 * bufClear() for that).
 */
Buffer *bufRewind(Buffer *buf)
{
    if (buf->data != NULL) buf->data[0] = 0;

    buf->used = 0;

    return buf;
}

/*
 * Get the number of valid bytes in <buf>.
 */
size_t bufLen(const Buffer *buf)
{
    return buf->used;
}

/*
 * Return TRUE if <buf> is empty, otherwise FALSE.
 */
bool bufIsEmpty(const Buffer *buf)
{
    return buf->used == 0;
}

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
Buffer *bufCat(Buffer *base, const Buffer *addition)
{
    bufAdd(base, bufGet(addition), bufLen(addition));

    return base;
}

/*
 * Trim <left> bytes from the left and <right> bytes from the right of <buf>.
 */
Buffer *bufTrim(Buffer *buf, size_t left, size_t right)
{
    if (buf->data == NULL) return buf;

    if (left  > buf->used) left = buf->used;
    if (right > buf->used - left) right = buf->used - left;

    memmove(buf->data, buf->data + left, buf->used - left - right);

    buf->used -= (left + right);

    *(buf->data + buf->used) = '\0';

    return buf;
}

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right>, either in size, or (when both have the same size) according to
 * memcmp().
 */
int bufCompare(const Buffer *left, const Buffer *right)
{
    int len1 = bufLen(left);
    int len2 = bufLen(right);

    if (len1 < len2)
        return -1;
    else if (len2 > len1)
        return 1;
    else
        return memcmp(bufGet(left), bufGet(right), len1);
}

/*
 * This function does the same as vstrpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufVaPack(Buffer *buf, va_list ap)
{
    if (buf->data == NULL) bufInit(buf);

    while (1) {
        va_list ap_copy;

        int required, available = buf->size - buf->used;

        va_copy(ap_copy, ap);
        required = vstrpack(buf->data + buf->used, available, ap_copy);
        va_end(ap_copy);

        if (available >= required + 1) {    // Room enough?
            buf->used += required;          // Yes! Update actual length...
            buf->data[buf->used] = '\0';    // ... and add a null-byte.
            break;
        }

        /* Not enough room: increase size until it fits. */

        while (buf->size < buf->used + required + 1) {
            buf->size *= 2;
        }

        /* Then realloc and try again. */

        buf->data = realloc(buf->data, buf->size);
    }

    return buf;
}

/*
 * This function does the same as strpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufPack(Buffer *buf, ...)
{
    va_list ap;

    va_start(ap, buf);
    bufVaPack(buf, ap);
    va_end(ap);

    return buf;
}

/*
 * This function does the same as vstrunpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufVaUnpack(Buffer *buf, va_list ap)
{
    if (buf->data == NULL) bufInit(buf);

    vstrunpack(buf->data, buf->used, ap);

    return buf;
}

/*
 * This function does the same as strunpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufUnpack(Buffer *buf, ...)
{
    va_list ap;

    va_start(ap, buf);
    bufVaUnpack(buf, ap);
    va_end(ap);

    return buf;
}

/*
 * This function assists in building textual lists of the form "Tom, Dick and
 * Harry". Call it three times with the arguments "Tom", "Dick" and "Harry".
 * Set sep1 to ", " and sep2 to " and ". Set is_first to TRUE when passing in
 * "Tom", set is_last to TRUE when passing in "Harry", set them both to FALSE
 * for "Dick". Returns the same pointer to <buf> that was passed in.
 */
__attribute__((format (printf, 6, 7)))
Buffer *bufList(Buffer *buf, const char *sep1, const char *sep2,
        int is_first, int is_last, const char *fmt, ...)
{
    va_list ap;

    if (!is_first) {
        if (is_last)
            bufAddF(buf, "%s", sep2);
        else
            bufAddF(buf, "%s", sep1);
    }

    va_start(ap, fmt);
    bufAddV(buf, fmt, ap);
    va_end(ap);

    return buf;
}

/*
 * Return true if <buf> starts with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool bufStartsWith(const Buffer *buf, const char *fmt, ...)
{
    va_list ap;
    char *pat;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&pat, fmt, ap);
    dbgAssert(stderr, r >= 0, "vasprintf failed (%s)\n", strerror(errno));
    va_end(ap);

    r = strncmp(bufGet(buf), pat, strlen(pat));

    free(pat);

    return r == 0;
}

/*
 * Return true if <buf> ends with the text created by <fmt> and the
 * subsequent parameters, otherwise false.
 */
__attribute__((format (printf, 2, 3)))
bool bufEndsWith(const Buffer *buf, const char *fmt, ...)
{
    va_list ap;
    char *pat;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&pat, fmt, ap);
    dbgAssert(stderr, r >= 0, "vasprintf failed (%s)\n", strerror(errno));
    va_end(ap);

    r = strcmp(bufGet(buf) + bufLen(buf) - strlen(pat), pat);

    free(pat);

    return r == 0;
}
