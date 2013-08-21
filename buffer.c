/*
 * buffer.c: Provides growing byte buffers. Buffers always include a trailing null-byte to make it
 * easier to handle them as strings.
 *
 * Copyright:	(c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
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
#include "utils.h"
#include "defs.h"
#include "debug.h"

#define INITIAL_SIZE 16

/*
 * Increase the size of <buf> to allow for another <len> bytes.
 */
static void buf_enlarge(Buffer *buf, size_t len)
{
    int new_len;

    while (buf->act_len + len + 1 > buf->max_len) {
        if (buf->max_len == 0)
            new_len = INITIAL_SIZE;
        else
            new_len = 2 * buf->max_len;

        buf->data = realloc(buf->data, new_len);

        dbgAssert(stderr, buf->data != NULL, "Could not (re-)allocate buffer data");

        buf->max_len = new_len;
    }
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
    buf->max_len = INITIAL_SIZE;
    buf->data = calloc(1, buf->max_len);
    buf->act_len = 0;

    return buf;
}

/*
 * Reset buffer <buf> to a virgin state, freeing its internal data. Use this if you have an
 * automatically allocated Buffer and want to completely discard its contents before it goes out of
 * scope. If <buf> was dynamically allocated (using bufCreate() above) you may free() it after
 * calling this function (or you could simply call bufDestroy()).
 */
void bufReset(Buffer *buf)
{
    if (buf->data) free(buf->data);

    memset(buf, 0, sizeof(Buffer));
}

/*
 * Detach and return the contents of <buf>, and reinitialize the buffer. The caller is responsible
 * for the returned data after this and should free() it when finished.
 */
char *bufDetach(Buffer *buf)
{
    char *data = buf->data;

    buf->data = NULL;
    buf->max_len = 0;
    buf->act_len = 0;

    return data ? data : strdup("");
}

/*
 * Destroy <buf>, but save and return its contents. The caller is responsible for the returned data
 * after this and should free() it when finished.
 */
char *bufFinish(Buffer *buf)
{
    char *data = buf->data;

    free(buf);

    return data ? data : strdup("");
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
Buffer *bufAdd(Buffer *buf, const void *data, size_t len)
{
    dbgAssert(stderr, len >= 0, "bufAdd called with length %lu", len);

    buf_enlarge(buf, len);

    memcpy(buf->data + buf->act_len, data, len);

    buf->act_len += len;

    buf->data[buf->act_len] = '\0';

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
 * Append a string to <buf>, formatted according to <fmt> and with the subsequent parameters
 * contained in <ap>.
 */
Buffer *bufAddV(Buffer *buf, const char *fmt, va_list ap)
{
    size_t size;
    va_list my_ap;

    va_copy(my_ap, ap);
    size = vsnprintf(NULL, 0, fmt, my_ap);
    va_end(my_ap);

    buf_enlarge(buf, size + 1);

    va_copy(my_ap, ap);
    vsnprintf(buf->data + buf->act_len, size + 1, fmt, my_ap);
    va_end(my_ap);

    buf->act_len += size;

    return buf;
}

/*
 * Append a string to <buf>, formatted according to <fmt> and the subsequent parameters.
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
 * Replace <buf> with the <len> bytes starting at <data>.
 */
Buffer *bufSet(Buffer *buf, const void *data, size_t len)
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
 * Set <buf> to a string formatted according to <fmt> and the subsequent parameters.
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
 * Replace <buf> with a string formatted according to <fmt> and the subsequent parameters contained
 * in <ap>.
 */
Buffer *bufSetV(Buffer *buf, const char *fmt, va_list ap)
{
    bufClear(buf);

    return bufAddV(buf, fmt, ap);
}

/*
 * Get a pointer to the data from <buf>. Find the size of the returned data using bufLen(). Note
 * that this returns a direct pointer to the data in <buf>. You are not supposed to change it (hence
 * the const keyword).
 */
const char *bufGet(const Buffer *buf)
{
    return buf->data ? buf->data : "";
}

/*
 * Clear the data in <buf>. Not that this function deviates from the "ground rules" in README, in
 * that it does not free <buf>'s internal data, so you shouldn't free() <buf> after this. If you
 * want to do that, use bufReset() instead.
 */
Buffer *bufClear(Buffer *buf)
{
    if (buf->data != NULL) buf->data[0] = 0;

    buf->act_len = 0;

    return buf;
}

/*
 * Get the number of valid bytes in <buf>.
 */
int bufLen(const Buffer *buf)
{
    return buf->act_len;
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

    if (left  > buf->act_len) left = buf->act_len;
    if (right > buf->act_len - left) right = buf->act_len - left;

    memmove(buf->data, buf->data + left, buf->act_len - left - right);

    buf->act_len -= (left + right);

    *(buf->data + buf->act_len) = '\0';

    return buf;
}

/*
 * Return -1, 1 or 0 if <left> is smaller than, greater than or equal to <right>, either in size, or
 * (when both have the same size) according to memcmp().
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

        int required, available = buf->max_len - buf->act_len;

        va_copy(ap_copy, ap);
        required = vstrpack(buf->data + buf->act_len, available, ap_copy);
        va_end(ap_copy);

        if (available >= required + 1) {    /* Room for "required" plus a null-byte? */
            buf->act_len += required;       /* Yes! Update actual length...          */
            buf->data[buf->act_len] = '\0'; /* ... and add the null-byte.            */
            break;
        }

        /* Not enough room: increase max_len until it fits. */

        while (buf->max_len < buf->act_len + required + 1) {
            buf->max_len *= 2;
        }

        /* Then realloc and try again. */

        buf->data = realloc(buf->data, buf->max_len);
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
 * This function does the same as strunpack from utils.[ch] but on a
 * Buffer instead of a char *.
 */
Buffer *bufVaUnpack(Buffer *buf, va_list ap)
{
    if (buf->data == NULL) bufInit(buf);

    vstrunpack(buf->data, buf->act_len, ap);

    return buf;
}

/*
 * This function does the same as vstrunpack from utils.[ch] but on a
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
 * This function assists in building textual lists of the form "Tom, Dick and Harry". Call it three
 * times with the arguments "Tom", "Dick" and "Harry". Set sep1 to ", " and sep2 to " and ". Set
 * is_first to TRUE when passing in "Tom", set is_last to TRUE when passing in "Harry", set them
 * both to FALSE for "Dick". Returns the same pointer to <buf> that was passed in.
 */
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

#ifdef TEST
static int errors = 0;

int main(int argc, char *argv[])
{
    Buffer buf1 = { };
    Buffer buf2 = { };
    Buffer *buf3;

    bufClear(&buf1);

    bufAdd(&buf1, "ABCDEF", 3);

    make_sure_that(bufLen(&buf1) == 3);
    make_sure_that(strcmp(bufGet(&buf1), "ABC") == 0);

    bufAddC(&buf1, 'D');

    make_sure_that(bufLen(&buf1) == 4);
    make_sure_that(strcmp(bufGet(&buf1), "ABCD") == 0);

    bufAddF(&buf1, "%d", 1234);

    make_sure_that(bufLen(&buf1) == 8);
    make_sure_that(strcmp(bufGet(&buf1), "ABCD1234") == 0);

    bufSet(&buf1, "ABCDEF", 3);

    make_sure_that(bufLen(&buf1) == 3);
    make_sure_that(strcmp(bufGet(&buf1), "ABC") == 0);

    bufSetC(&buf1, 'D');

    make_sure_that(bufLen(&buf1) == 1);
    make_sure_that(strcmp(bufGet(&buf1), "D") == 0);

    bufSetF(&buf1, "%d", 1234);

    make_sure_that(bufLen(&buf1) == 4);
    make_sure_that(strcmp(bufGet(&buf1), "1234") == 0);

    bufClear(&buf1);

    make_sure_that(bufLen(&buf1) == 0);
    make_sure_that(strcmp(bufGet(&buf1), "") == 0);

    bufSet(&buf1, "ABC", 3);
    bufSet(&buf2, "DEF", 3);

    buf3 = bufCat(&buf1, &buf2);

    make_sure_that(&buf1 == buf3);

    make_sure_that(bufLen(&buf1) == 6);
    make_sure_that(strcmp(bufGet(&buf1), "ABCDEF") == 0);

    make_sure_that(bufLen(&buf2) == 3);
    make_sure_that(strcmp(bufGet(&buf2), "DEF") == 0);

    bufSetF(&buf1, "ABCDEF");

    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 0, 0)), "ABCDEF") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 1, 0)), "BCDEF") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 0, 1)), "BCDE") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 1, 1)), "CD") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 3, 3)), "") == 0);

    bufPack(&buf1,
           PACK_INT8,   0x01,
           PACK_INT16,  0x0123,
           PACK_INT32,  0x01234567,
           PACK_INT64,  0x0123456789ABCDEF,
           PACK_FLOAT,  0.0,
           PACK_DOUBLE, 0.0,
           PACK_STRING, "Hoi1",
           PACK_DATA,   "Hoi2", 4,
           PACK_RAW,    "Hoi3", 4,
           END);

    make_sure_that(bufLen(&buf1) == 47);
    make_sure_that(memcmp(bufGet(&buf1),
        "\x01"
        "\x01\x23"
        "\x01\x23\x45\x67"
        "\x01\x23\x45\x67\x89\xAB\xCD\xEF"
        "\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x04Hoi1"
        "\x00\x00\x00\x04Hoi2"
        "Hoi3", 47) == 0);

    {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        float f32;
        double f64;
        char *string, *data, raw[4];
        int data_size;

        bufUnpack(&buf1,
                PACK_INT8,   &u8,
                PACK_INT16,  &u16,
                PACK_INT32,  &u32,
                PACK_INT64,  &u64,
                PACK_FLOAT,  &f32,
                PACK_DOUBLE, &f64,
                PACK_STRING, &string,
                PACK_DATA,   &data, &data_size,
                PACK_RAW,    &raw, 4,
                END);

        make_sure_that(u8  == 0x01);
        make_sure_that(u16 == 0x0123);
        make_sure_that(u32 == 0x01234567);
        make_sure_that(u64 == 0x0123456789ABCDEF);
        make_sure_that(f32 == 0.0);
        make_sure_that(f64 == 0.0);
        make_sure_that(memcmp(string, "Hoi1", 5) == 0);
        make_sure_that(memcmp(data, "Hoi2", 4) == 0);
        make_sure_that(data_size == 4);
        make_sure_that(memcmp(raw, "Hoi3", 4) == 0);

    }

    {
        const char *name[] = { "Mills", "Berry", "Buck", "Stipe" };

        bufClear(&buf1);

        bufList(&buf1, ", ", " and ", TRUE, TRUE, "%s", name[0]);

        make_sure_that(strcmp(bufGet(&buf1), "Mills") == 0);

        bufClear(&buf1);

        bufList(&buf1, ", ", " and ", TRUE, FALSE, "%s", name[0]);
        bufList(&buf1, ", ", " and ", FALSE, TRUE, "%s", name[1]);

        make_sure_that(strcmp(bufGet(&buf1), "Mills and Berry") == 0);

        bufClear(&buf1);

        bufList(&buf1, ", ", " and ", TRUE,  FALSE, "%s", name[0]);
        bufList(&buf1, ", ", " and ", FALSE, FALSE, "%s", name[1]);
        bufList(&buf1, ", ", " and ", FALSE, TRUE,  "%s", name[2]);

        make_sure_that(strcmp(bufGet(&buf1), "Mills, Berry and Buck") == 0);

        bufClear(&buf1);

        bufList(&buf1, ", ", " and ", TRUE,  FALSE, "%s", name[0]);
        bufList(&buf1, ", ", " and ", FALSE, FALSE, "%s", name[1]);
        bufList(&buf1, ", ", " and ", FALSE, FALSE, "%s", name[2]);
        bufList(&buf1, ", ", " and ", FALSE, TRUE,  "%s", name[3]);

        make_sure_that(strcmp(bufGet(&buf1), "Mills, Berry, Buck and Stipe") == 0);
    }

    bufReset(&buf1);
    bufReset(&buf2);

    return errors;
}
#endif
