/*
 * Provides growing byte buffers.
 *
 * Copyright:	(c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: buffer.c 281 2012-04-12 19:30:39Z jacco $
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

#include "buffer.h"

#define INITIAL_SIZE 1024

/*
 * Encode buffer <value> into <buf>.
 */
int bufEncode(Buffer *buf, const Buffer *value)
{
    uint64_t len = bufLen(value);
    int zeros, shift = 8 * sizeof(uint64_t) - 8;
    uint8_t bytes = 0, byte[sizeof(uint64_t)];

    for (shift = 8 * sizeof(uint64_t) - 8; shift >= 0; shift -= 8) {
        byte[bytes++] = (len >> shift) & 0xFF;
    }

    for (zeros = 0; zeros < sizeof(uint64_t); zeros++) {
        if (byte[zeros] != 0) break;
    }

    bytes -= zeros;

    bufAddC(buf, bytes);

    bufAdd(buf, byte + zeros, bytes);

    bufCat(buf, value);

    return 0;
}

/*
 * Get a binary-encoded buffer from <buf> and store it in <value>. If
 * succesful, the first part where the int8_t was stored will be
 * stripped from <buf>.
 */
int bufDecode(Buffer *buf, Buffer *value)
{
    const char *ptr = bufGet(buf);
    int remaining = bufLen(buf);

    if (bufExtract(&ptr, &remaining, value) == 0) {
        bufTrim(buf, bufLen(buf) - remaining, 0);
        return 0;
    }

    return 1;
}

/*
 * Get an encoded buffer from <ptr> (with <remaining> bytes
 * remaining) and store it in <value>.
 */
int bufExtract(const char **ptr, int *remaining, Buffer *value)
{
    const char *my_ptr = *ptr;
    int i, my_remaining = *remaining;

    uint64_t len;
    uint8_t byte, bytes;

    if (my_remaining < 1) return 1;

    bytes = *my_ptr;

    my_ptr++;
    my_remaining--;

    if (my_remaining < bytes) return 1;

    len = 0;

    for (i = 0; i < bytes; i++) {
        byte = *my_ptr;

        len <<= 8;
        len += byte;

        my_ptr++;
        my_remaining--;
    }

    if (my_remaining < len) return 1;

    bufSet(value, my_ptr, len);

    my_ptr += len;
    my_remaining -= len;

    *ptr = my_ptr;
    *remaining = my_remaining;

    return 0;
}

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to
 * <right> (according to memcmp()).
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
 * Create an empty buffer.
 */
Buffer *bufCreate(void)
{
    return calloc(1, sizeof(Buffer));
}

/*
 * Initialize buffer <buf>.
 */
Buffer *bufInit(Buffer *buf)
{
    buf->size = INITIAL_SIZE;
    buf->data = calloc(1, (size_t) buf->size);
    buf->used = 1;

    return buf;
}

/*
 * Detach the contents of <buf>. The buffer is re-initialized and the
 * old contents are returned. The caller is responsible for the contents
 * after this and should free() them when finished.
 */
char *bufDetach(Buffer *buf)
{
    char *data = buf->data;

    bufInit(buf);

    return data;
}

/*
 * Destroy <buf>, but save and return its contents. The caller is responsible
 * for the contents after this and should free() them when finished.
 */
char *bufFinish(Buffer *buf)
{
   char *data = buf->data;

   free(buf);

   return data;
}

/*
 * Destroy <buf>, together with the data it contains.
 */
void bufDestroy(Buffer *buf)
{
    if (buf->data) free(buf->data);

    free(buf);
}

/*
 * Add <len> bytes, starting at <data> to <buf>.
 */
Buffer *bufAdd(Buffer *buf, const void *data, int len)
{
    if (buf->data == NULL) bufInit(buf);

    while (buf->used + len > buf->size) buf->size *= 2;

    buf->data = realloc(buf->data, buf->size);

    assert(buf->data);

    memcpy(buf->data + buf->used - 1, data, len);

    buf->used += len;

    buf->data[buf->used - 1] = '\0';

    return buf;
}

/* Add the single character <c>. */

Buffer *bufAddC(Buffer *buf, char c)
{
   bufAdd(buf, &c, 1);

   return buf;
}

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters.
 */
Buffer *bufAddF(Buffer *buf, const char *fmt, ...)
{
   va_list ap;

   va_start(ap, fmt);

   bufAddV(buf, fmt, ap);

   va_end(ap);

   return buf;
}

/*
 * Append a string to <buf>, formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
Buffer *bufAddV(Buffer *buf, const char *fmt, va_list ap)
{
   va_list my_ap;

   int len;

   static int scratch_size = 80;
   static char *scratch_pad = NULL;

   if (scratch_pad == NULL) {
      scratch_pad = calloc(1, scratch_size);
   }

   while (1) {
      va_copy(my_ap, ap);
      len = vsnprintf(scratch_pad, scratch_size, fmt, my_ap);
      va_end(my_ap);

      if (len < scratch_size) break;

      scratch_size *= 2;

      scratch_pad = realloc(scratch_pad, scratch_size);
   }

   bufAdd(buf, scratch_pad, len);

   return buf;
}

/*
 * Replace <buf> with the <len> bytes starting at <data>.
 */
Buffer *bufSet(Buffer *buf, const void *data, int len)
{
    bufClear(buf);

    return bufAdd(buf, data, len);
}

/*
 * Set <buf> to the single character <c>.
 */
Buffer *bufSetC(Buffer *buf, char c)
{
   bufClear(buf);

   return bufAdd(buf, &c, 1);
}

/*
 * Set <buf> to a string formatted according to <fmt> and with the
 * subsequent parameters.
 */
Buffer *bufSetF(Buffer *buf, const char *fmt, ...)
{
   va_list ap;

   va_start(ap, fmt);

   bufSetV(buf, fmt, ap);

   va_end(ap);

   return buf;
}

/*
 * Replace <buf> with a string formatted according to <fmt> and with the
 * subsequent parameters contained in <ap>.
 */
Buffer *bufSetV(Buffer *buf, const char *fmt, va_list ap)
{
   bufClear(buf);

   return bufAddV(buf, fmt, ap);
}

/*
 * Get a pointer to the data from <buf>. Find the size of the buffer using
 * bufLen().
 */
const char *bufGet(const Buffer *buf)
{
    return buf->data;
}

/*
 * Clear the data in <buf>.
 */
Buffer *bufClear(Buffer *buf)
{
    if (buf->data == NULL) bufInit(buf);

    buf->data[0] = '\0';
    buf->used = 1;

    return buf;
}

/*
 * Get the number of valid bytes in <buf>.
 */
int bufLen(const Buffer *buf)
{
    return buf->used - 1;
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
Buffer *bufTrim(Buffer *buf, unsigned int left, unsigned int right)
{
    if (buf->data == NULL) bufInit(buf);

    if (left  > buf->used - 1) left = buf->used - 1;
    if (right > buf->used - left - 1) right = buf->used - left - 1;

    memmove(buf->data, buf->data + left, buf->used - left - right - 1);

    buf->used -= (left + right);

    *(buf->data + buf->used - 1) = '\0';

    return buf;
}

#ifdef TEST
static int errors = 0;

void _make_sure_that(const char *file, int line, const char *str, int val)
{
   if (!val) {
      fprintf(stderr, "%s:%d: Expression \"%s\" failed\n", file, line, str);
      errors++;
   }
}

#define make_sure_that(expr) _make_sure_that(__FILE__, __LINE__, #expr, (expr))

int main(int argc, char *argv[])
{
   Buffer *buf1 = bufCreate();
   Buffer *buf2 = bufCreate();
   Buffer *buf3;

   bufAdd(buf1, "ABCDEF", 3);

   make_sure_that(bufLen(buf1) == 3);
   make_sure_that(strcmp(bufGet(buf1), "ABC") == 0);

   bufAddC(buf1, 'D');

   make_sure_that(bufLen(buf1) == 4);
   make_sure_that(strcmp(bufGet(buf1), "ABCD") == 0);

   bufAddF(buf1, "%d", 1234);

   make_sure_that(bufLen(buf1) == 8);
   make_sure_that(strcmp(bufGet(buf1), "ABCD1234") == 0);

   bufSet(buf1, "ABCDEF", 3);

   make_sure_that(bufLen(buf1) == 3);
   make_sure_that(strcmp(bufGet(buf1), "ABC") == 0);

   bufSetC(buf1, 'D');

   make_sure_that(bufLen(buf1) == 1);
   make_sure_that(strcmp(bufGet(buf1), "D") == 0);

   bufSetF(buf1, "%d", 1234);

   make_sure_that(bufLen(buf1) == 4);
   make_sure_that(strcmp(bufGet(buf1), "1234") == 0);

   bufClear(buf1);

   make_sure_that(bufLen(buf1) == 0);
   make_sure_that(strcmp(bufGet(buf1), "") == 0);

   bufSet(buf1, "ABC", 3);
   bufSet(buf2, "DEF", 3);

   buf3 = bufCat(buf1, buf2);

   make_sure_that(buf1 == buf3);

   make_sure_that(bufLen(buf1) == 6);
   make_sure_that(strcmp(bufGet(buf1), "ABCDEF") == 0);

   make_sure_that(bufLen(buf2) == 3);
   make_sure_that(strcmp(bufGet(buf2), "DEF") == 0);

   bufSetF(buf1, "ABCDEF");

   make_sure_that(strcmp(bufGet(bufTrim(buf1, 0, 0)), "ABCDEF") == 0);
   make_sure_that(strcmp(bufGet(bufTrim(buf1, 1, 0)), "BCDEF") == 0);
   make_sure_that(strcmp(bufGet(bufTrim(buf1, 0, 1)), "BCDE") == 0);
   make_sure_that(strcmp(bufGet(bufTrim(buf1, 1, 1)), "CD") == 0);
   make_sure_that(strcmp(bufGet(bufTrim(buf1, 3, 3)), "") == 0);

   bufDestroy(buf1);
   bufDestroy(buf2);

   buf1 = bufCreate();
   buf2 = bufCreate();

   bufSet(buf1, "Hoi!", 4);
   bufEncode(buf2, buf1);

   make_sure_that(memcmp(bufGet(buf2), "\01\04Hoi!", 6) == 0);

   return 0;
}
#endif
