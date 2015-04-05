/*
 * utils.c: Description
 *
 * Copyright:   (c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <endian.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>

#include "defs.h"
#include "utils.h"
#include "buffer.h"

/*
 * Return the current stack depth.
 */
int stackdepth(void)
{
    void *bt_buffer[100];

    return backtrace(bt_buffer, sizeof(bt_buffer)) - 1;
}

/*
 * Output <level> levels of indent to <fp>.
 */
void findent(FILE *fp, int level)
{
    int i;

    for (i = 0; i < level; i++) {
        fputs("  ", fp);
    }
}

/*
 * Indented fprintf. Print output given by <fmt> and the following parameters, preceded by <indent>
 * levels of indent, to <fp>. Returns the number of characters printed (just like fprintf).
 */
int ifprintf(FILE *fp, int indent, const char *fmt, ...)
{
    int r;
    va_list ap;

    findent(fp, indent);

    va_start(ap, fmt);
    r = vfprintf(fp, fmt, ap);
    va_end(ap);

    return r;
}

#define HEXDUMP_BYTES_PER_LINE 16

/*
 * Dump <size> bytes from <data> into a new string buffer, using indent <indent>. The address of the
 * string buffer is returned through <str>, its length is this function's return value. Afterwards,
 * the caller is responsible for the string buffer and should free it when it is no longer needed.
 */
int ihexstr(char **str, int indent, const char *data, int size)
{
    Buffer *output = bufCreate();
    int i, begin = 0, end;

    while (begin < size) {
        bufAddF(output, "%*s", 2 * indent, "");

        end = MIN(begin + HEXDUMP_BYTES_PER_LINE, size);

        bufAddF(output, "%06X  ", begin);

        for (i = begin; i < end; i++) {
            bufAddF(output, "%02hhX ", data[i]);
        }

        for (i = end; i < begin + HEXDUMP_BYTES_PER_LINE; i++) {
            bufAdd(output, "   ", 3);
        }

        for (i = begin; i < end; i++) {
            bufAddC(output, isprint(data[i]) ? data[i] : '.');
        }

        bufAddC(output, '\n');

        begin = end;
    }

    size = bufLen(output);
    *str = bufFinish(output);

    return size;
}

/*
 * Dump <size> bytes from <data> as a hexdump to <fp>, with indent level <indent>.
 */
void ihexdump(FILE *fp, int indent, const char *data, int size)
{
    int i, offset = 0;
    char buffer[80];

    while (offset < size) {
        sprintf(buffer, "%06X  ", offset);

        for (i = offset; i < MIN(offset + HEXDUMP_BYTES_PER_LINE, size); i++) {
            sprintf(buffer + 8 + (i - offset) * 3, "%02hhX ", data[i]);
        }

        memset(buffer + 8 + (i - offset) * 3, ' ',
               sizeof(buffer) - (8 + (i - offset) * 3));

        for (i = offset; i < MIN(offset + HEXDUMP_BYTES_PER_LINE, size); i++) {
            sprintf(buffer + 9 + HEXDUMP_BYTES_PER_LINE * 3 + (i - offset),
                    "%c", isprint(data[i]) ? data[i] : '.');
        }

        findent(fp, indent);
        fputs(buffer, fp);
        fputc('\n', fp);

        offset += HEXDUMP_BYTES_PER_LINE;
    }
}

/*
 * Dump the file descriptors up to <nfds> in <fds> to <fp>, preceded by <intro> (if <intro> is not
 * NULL).
 */
void dumpfds(FILE *fp, const char *intro, int nfds, fd_set *fds)
{
    int fd;

    if (intro != NULL) {
        fputs(intro, fp);
    }

    for (fd = 0; fd < nfds; fd++) {
        if (FD_ISSET(fd, fds)) {
            fprintf(fp, " %d", fd);
        }
    }

    fputc('\n', fp);
}

/*
 * Duplicate <size> bytes starting at <src> and return a pointer to the
 * duplicate.
 */
void *memdup(const void *src, unsigned int size)
{
    char *dupe = malloc(size);

    memcpy(dupe, src, size);

    return dupe;
}

/*
 * Convert the double timestamp <t> to a struct timeval in <tv>.
 */
void dtotv(double t, struct timeval *tv)
{
    tv->tv_sec  = t;
    tv->tv_usec = 1000000 * fmod(t, 1.0);
}

/*
 * Return the timestamp in <tv> as a double.
 */
double tvtod(const struct timeval *tv)
{
    return tv->tv_sec + tv->tv_usec / 1000000.0;
}

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00 UTC) as a double.
 */
double nowd(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tvtod(&tv);
}

/*
 * Pack <data>, whose size is <size>, into <*dest> which has
 * <*remaining> bytes remaining. Update <*dest> and <*remaining> to
 * the new situation, *even* if there is not enough room (although
 * nothing is actually written in that case).
 */
static void pack(void *data, int size, char **dest, int *remaining)
{
    if (*remaining >= size) memcpy(*dest, data, size);

    *dest += size;
    *remaining -= size;
}

/*
 * Pack data from <ap> into <str>, which has size <size>.
 *
 * The pack functions take type/value pairs (closed by END) to specify what to pack into the string.
 * The types, values and packed data are as follows:
 *
 * type         value           packs
 * ----         -----           -----
 * PACK_INT8	int		1 byte int
 * PACK_INT16	int		2 byte int
 * PACK_INT32	int		4 byte int
 * PACK_INT64	uint64_t	8 byte int
 * PACK_FLOAT	double		4 byte float
 * PACK_DOUBLE	double		8 byte double
 * PACK_STRING	char *		4-byte length (from strlen) followed by as many bytes.
 * PACK_DATA	char *, uint	4-byte length (as given) followed by as many bytes.
 * PACK_RAW	char *, uint	Raw bytes using length as given.
 *
 * All ints (including the lengths) are packed with big-endian byte order.
 *
 * All pack functions return the number of bytes necessary to pack the given data, which may be more
 * than <size>. In that case they are telling you that more bytes were needed than you gave them,
 * and you should call the function again with a bigger data buffer. It will, however, never write
 * more than <size> bytes into <str>.
 */
int vstrpack(char *str, int size, va_list ap)
{
    int type;

    char *p = str;

    while ((type = va_arg(ap, int)) != END) {
        switch(type) {
        case PACK_INT8:
            {
                uint8_t u8 = va_arg(ap, unsigned int);
                pack(&u8, sizeof(u8), &p, &size);
            }
            break;
        case PACK_INT16:
            {
                uint16_t u16 = htobe16(va_arg(ap, unsigned int));
                pack(&u16, sizeof(u16), &p, &size);
            }
            break;
        case PACK_INT32:
            {
                uint32_t u32 = htobe32(va_arg(ap, unsigned int));
                pack(&u32, sizeof(u32), &p, &size);
            }
            break;
        case PACK_INT64:
            {
                uint64_t u64 = htobe64(va_arg(ap, uint64_t));
                pack(&u64, sizeof(u64), &p, &size);
            }
            break;
        case PACK_FLOAT:
            {
                float f32 = va_arg(ap, double);
                uint32_t u32 = *((uint32_t *) &f32);

                u32 = htobe32(u32);

                pack(&u32, sizeof(u32), &p, &size);
            }
            break;
        case PACK_DOUBLE:
            {
                double f64 = va_arg(ap, double);
                uint64_t u64 = *((uint64_t *) &f64);

                u64 = htobe64(u64);

                pack(&u64, sizeof(u64), &p, &size);
            }
            break;
        case PACK_STRING:
            {
                char *s = va_arg(ap, char *);
                uint32_t len_h  = strlen(s);
                uint32_t len_be = htobe32(len_h);

                pack(&len_be, sizeof(len_be), &p, &size);
                pack(s, len_h, &p, &size);
            }
            break;
        case PACK_DATA:
            {
                char *s = va_arg(ap, char *);
                uint32_t len_h  = va_arg(ap, unsigned int);
                uint32_t len_be = htobe32(len_h);

                pack(&len_be, sizeof(len_be), &p, &size);
                pack(s, len_h, &p, &size);
            }
            break;
        case PACK_RAW:
            {
                char *s = va_arg(ap, char *);
                uint32_t len_h  = va_arg(ap, unsigned int);

                pack(s, len_h, &p, &size);
            }
            break;
        default:
            fprintf(stderr, "Unknown pack type %d\n", type);
            abort();
        }
    }

    return p - str;
}

/*
 * Pack data into <str>, using a variable number of arguments.
 */
int strpack(char *str, int size, ...)
{
    int r;
    va_list ap;

    va_start(ap, size);
    r = vstrpack(str, size, ap);
    va_end(ap);

    return r;
}

/*
 * This function does the same as vstrpack, but it will allocate a sufficiently sized data buffer
 * for you and return it through <str>.
 */
int vastrpack(char **str, va_list ap)
{
    va_list my_ap;

    int len = 0;

    /* First find out how much room I need. */

    va_copy(my_ap, ap);
    len = vstrpack(NULL, len, my_ap);
    va_end(my_ap);

    /* Allocate... */

    if ((*str = malloc(len)) == NULL) return -1;

    /* ... and now do it for realz. */

    va_copy(my_ap, ap);
    len = vstrpack(*str, len, my_ap);
    va_end(my_ap);

    return len;
}

/*
 * This function does the same as strpack, but it will allocate a sufficiently sized data buffer for
 * you and return it through <str>.
 */
int astrpack(char **str, ...)
{
    int r;
    va_list ap;

    va_start(ap, str);
    r = vastrpack(str, ap);
    va_end(ap);

    return r;
}

/*
 * Unpack data from <str>, which has length <size>.
 *
 * The unpack functions take type/pointer pairs (closed by END) where data is extracted from <str>
 * and put into the addresses that the pointers point to. The types, pointers and unpacked data are
 * as follows:
 *
 * type         pointer         unpacks
 * ----         -------         -----
 * PACK_INT8	uint8_t *	1 byte int
 * PACK_INT16	uint16_t *	2 byte int
 * PACK_INT32	uint32_t *	4 byte int
 * PACK_INT64	uint64_t *	8 byte int
 * PACK_FLOAT	float *		4 byte float
 * PACK_DOUBLE	double *	8 byte double
 * PACK_STRING	char **		4-byte length, followed by as many bytes.
 * PACK_DATA	char **, uint *	4-byte length, followed by as many bytes.
 * PACK_RAW	char *, uint	As many raw bytes as given.
 *
 * Note: PACK_STRING and PACK_DATA allocate space to put the data in, and it is the caller's
 * responsibility to free that space again. PACK_STRING creates a null-terminated string. PACK_DATA
 * requires an additional int * where it writes the length of the allocated data.
 *
 * This function returns the minimum number of bytes necessary to unpack the given fields, which may
 * be more than <size>. "Minimum" in this case means assuming all PACK_STRING and PACK_DATA fields
 * have length 0.
 */
int vstrunpack(const char *str, int size, va_list ap)
{
    int type;

    const char *ptr = str;

    while ((type = va_arg(ap, int)) != END) {
        switch(type) {
        case PACK_INT8:
            {
                uint8_t *u8 = va_arg(ap, uint8_t *);
                if (u8 != NULL && size >= sizeof(uint8_t)) *u8 = *((uint8_t *) ptr);
                ptr += sizeof(uint8_t);
                size -= sizeof(uint8_t);
            }
            break;
        case PACK_INT16:
            {
                uint16_t *u16 = va_arg(ap, uint16_t *);
                if (u16 != NULL && size >= sizeof(uint16_t)) *u16 = be16toh(*((uint16_t *) ptr));
                ptr += sizeof(uint16_t);
                size -= sizeof(uint16_t);
            }
            break;
        case PACK_INT32:
            {
                uint32_t *u32 = va_arg(ap, uint32_t *);
                if (u32 != NULL && size >= sizeof(uint32_t)) *u32 = be32toh(*((uint32_t *) ptr));
                ptr += sizeof(uint32_t);
                size -= sizeof(uint32_t);
            }
            break;
        case PACK_INT64:
            {
                uint64_t *u64 = va_arg(ap, uint64_t *);
                if (u64 != NULL && size >= sizeof(uint64_t)) *u64 = be64toh(*((uint64_t *) ptr));
                ptr += sizeof(uint64_t);
                size -= sizeof(uint64_t);
            }
            break;
        case PACK_FLOAT:
            {
                float *f32 = va_arg(ap, float *);
                if (f32 != NULL && size >= sizeof(float)) {
                    uint32_t u32 = *((uint32_t *) ptr);
                    u32 = be32toh(u32);
                    *f32 = *((float *) &u32);
                }
                ptr += sizeof(float);
                size -= sizeof(float);
            }
            break;
        case PACK_DOUBLE:
            {
                double *f64 = va_arg(ap, double *);
                if (f64 != NULL && size >= sizeof(double)) {
                    uint64_t u64 = *((uint64_t *) ptr);
                    u64 = be64toh(u64);
                    *f64 = *((double *) &u64);
                }
                ptr += sizeof(double);
                size -= sizeof(double);
            }
            break;
        case PACK_STRING:
            {
                int len = 0;
                char **strpp = va_arg(ap, char **);

                if (size >= sizeof(uint32_t)) len = be32toh(*((uint32_t *) ptr));
                ptr += sizeof(uint32_t);
                size -= sizeof(uint32_t);

                if (strpp != NULL && size >= len) *strpp = strndup(ptr, len);
                ptr += len;
                size -= len;
            }
            break;
        case PACK_DATA:
            {
                char **strpp = va_arg(ap, char **);
                uint32_t len = 0, *lenp = va_arg(ap, uint32_t *);

                if (size >= sizeof(uint32_t)) {
                    len = be32toh(*((uint32_t *) ptr));
                }

                ptr += sizeof(uint32_t);
                size -= sizeof(uint32_t);

                if (lenp != NULL) {
                    *lenp = len;
                }

                if (strpp != NULL && size >= len) {
                    *strpp = malloc(len);
                    memcpy(*strpp, ptr, len);
                }

                ptr += len;
                size -= len;
            }
            break;
        case PACK_RAW:
            {
                char *strp = va_arg(ap, char *);
                int len = va_arg(ap, unsigned int);

                if (strp != NULL && size >= len) {
                    memcpy(strp, ptr, len);
                }

                ptr  += len;
                size -= len;
            }
            break;
        }
    }

    return ptr - str;
}

/*
 * Unpack data from <str> (which has size <size>) into the pointers following "size".
 */
int strunpack(const char *str, int size, ...)
{
    int r;
    va_list ap;

    va_start(ap, size);
    r = vstrunpack(str, size, ap);
    va_end(ap);

    return r;
}

/*
 * Test <val>. If it is FALSE, print that fact, along with the textual representation of <val> which
 * is in <str>, the file and line on which the error occurred in <file> and <line> to stderr, and
 * increase <*errors> by 1. Called by the make_sure_that() macro, less useful on its own.
 */
void _make_sure_that(const char *file, int line, int *errors, const char *str, int val)
{
    if (!val) {
        fprintf(stderr, "%s:%d: Expression \"%s\" failed\n", file, line, str);
        (*errors)++;
    }
}

#ifdef TEST
int errors = 0;

int main(int argc, char *argv[])
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    double f64;
    float f32;
    char *sp, *dp, *buf_p;
    char raw_buf[6] = { 0 };
    int len;

    struct timeval tv;

    char buffer[64] = { 0 };

    unsigned char expected[] = {
        1,
        0, 2,
        0, 0, 0, 3,
        0, 0, 0, 0, 0, 0, 0, 4,
        0x3F, 0x80, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0, 0, 0, 3, 'H', 'o', 'i',
        0, 0, 0, 5, 'H', 'e', 'l', 'l', 'o',
        'W', 'o', 'r', 'l', 'd'
    };

    int r = strpack(buffer, sizeof(buffer),
            PACK_INT8,      1,
            PACK_INT16,     2,
            PACK_INT32,     3,
            PACK_INT64,     4LL,
            PACK_FLOAT,     1.0,
            PACK_DOUBLE,    2.0,
            PACK_STRING,    "Hoi",
            PACK_DATA,      "Hello", 5,
            PACK_RAW,       "World xxx", 5,
            END);

    make_sure_that(r == 48);

    make_sure_that(memcmp(buffer, expected, 48) == 0);

    r = astrpack(&buf_p,
            PACK_INT8,      1,
            PACK_INT16,     2,
            PACK_INT32,     3,
            PACK_INT64,     4LL,
            PACK_FLOAT,     1.0,
            PACK_DOUBLE,    2.0,
            PACK_STRING,    "Hoi",
            PACK_DATA,      "Hello", 5,
            PACK_RAW,       "World xxx", 5,
            END);

    make_sure_that(r == 48);

    make_sure_that(memcmp(buf_p, expected, 48) == 0);

    r = strunpack(buffer, sizeof(buffer),
            PACK_INT8,      &u8,
            PACK_INT16,     &u16,
            PACK_INT32,     &u32,
            PACK_INT64,     &u64,
            PACK_FLOAT,     &f32,
            PACK_DOUBLE,    &f64,
            PACK_STRING,    &sp,
            PACK_DATA,      &dp, &len,
            PACK_RAW,       raw_buf, 5,
            END);

    make_sure_that(u8  == 1);
    make_sure_that(u16 == 2);
    make_sure_that(u32 == 3);
    make_sure_that(u64 == 4);
    make_sure_that(f32 == 1.0);
    make_sure_that(f64 == 2.0);
    make_sure_that(strcmp(sp, "Hoi") == 0);
    make_sure_that(len == 5);
    make_sure_that(memcmp(dp, "Hello", 5) == 0);
    make_sure_that(memcmp(raw_buf, "World", 5) == 0);
    make_sure_that(r == 48);

    r = strunpack(buffer, sizeof(buffer),
            PACK_INT8,      NULL,
            PACK_INT16,     NULL,
            PACK_INT32,     NULL,
            PACK_INT64,     NULL,
            PACK_FLOAT,     NULL,
            PACK_DOUBLE,    NULL,
            PACK_STRING,    NULL,
            PACK_DATA,      NULL, NULL,
            PACK_RAW,       NULL, 5,
            END);

    make_sure_that(r == 48);

    r = ihexstr(&sp, 1, "0123456789ABCD\n\r", 16);

    make_sure_that(r == 75);

    make_sure_that(strncmp(sp,
                "  000000  30 31 32 33 34 35 36 37 38 39 41 42 43 44 0A 0D 0123456789ABCD..\n",
                r) == 0);

    tv.tv_sec  = 0;
    tv.tv_usec = 999999;

    make_sure_that(tvtod(&tv) == 0.999999);

    tv.tv_sec  = 1;
    tv.tv_usec = 0;

    make_sure_that(tvtod(&tv) == 1.0);

    dtotv(0.999999, &tv);

    make_sure_that(tv.tv_sec == 0 && tv.tv_usec == 999999);

    dtotv(1.0, &tv);

    make_sure_that(tv.tv_sec == 1 && tv.tv_usec == 0);

    return errors;
}
#endif
