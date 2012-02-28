/*
 * Provides growing byte buffers.
 *
 * Copyright:	(c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: buffer.c 11448 2009-10-23 12:41:50Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "buffer.h"

#define INITIAL_SIZE 1024

struct Buffer {
    char *data;
    int   size;   /* The number of bytes allocated. */
    int   used;   /* The number of bytes in use (incl. a trailing null byte). */
};

/*
 * Create an empty buffer.
 */
Buffer *bufCreate(void)
{
    Buffer *buf = calloc(1, sizeof(Buffer));

    assert(buf != NULL);

    buf->size = INITIAL_SIZE;
    buf->data = calloc(1, (size_t) buf->size);
    buf->used = 1;

    assert(buf->data);

    return buf;
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
 * Add <len> bytes, starting at <data> to <buf>.
 */
Buffer *bufAdd(Buffer *buf, const void *data, int len)
{
    while (buf->used + len > buf->size) {
        buf->size *= 2;
        buf->data = realloc(buf->data, buf->size);

        assert(buf->data);
    }

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

/* Replace <buf> with the single character <c>. */

Buffer *bufSetC(Buffer *buf, char c)
{
   bufClear(buf);

   return bufAdd(buf, &c, 1);
}

/*
 * Replace <buf> with a string formatted according to <fmt> and with the
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
char *bufGet(Buffer *buf)
{
    return buf->data;
}

/*
 * Clear the data in <buf>.
 */
Buffer *bufClear(Buffer *buf)
{
    buf->data[0] = '\0';
    buf->used = 1;

    return buf;
}

/*
 * Get the number of valid bytes in <buf>.
 */
int bufLen(Buffer *buf)
{
    return buf->used - 1;
}

/*
 * Concatenate <addition> onto <base> and return <base>.
 */
Buffer *bufCat(Buffer *base, Buffer *addition)
{
   bufAdd(base, bufGet(addition), bufLen(addition));

   return base;
}

/*
 * Trim <left> bytes from the left and <right> bytes from the right of <buf>.
 */
Buffer *bufTrim(Buffer *buf, unsigned int left, unsigned int right)
{
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

   return 0;
}
#endif
