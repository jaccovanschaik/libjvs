/*
 * utils.c: Various utility functions.
 *
 * utils.c is part of libjvs.
 *
 * Copyright:   (c) 2012-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: utils.c 507 2025-08-23 14:43:51Z jacco $
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
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <iconv.h>
#include <errno.h>
#include <regex.h>

#include "defs.h"
#include "utils.h"
#include "buffer.h"

static char *indent_string = NULL;

/*
 * Return the current stack depth.
 */
int stackdepth(void)
{
    void *bt_buffer[100];

    return backtrace(bt_buffer, sizeof(bt_buffer) / sizeof(bt_buffer[0])) - 1;
}

/*
 * Set the indent string used by findent and ifprintf.
 */
void set_indent_string(const char *str)
{
    if (indent_string == NULL) {
        free(indent_string);
    }

    indent_string = strdup(str);
}

/*
 * Output <level> levels of indent to <fp>.
 */
void findent(FILE *fp, int level)
{
    int i;

    if (indent_string == NULL) {
        set_indent_string("    ");
    }

    for (i = 0; i < level; i++) {
        fputs(indent_string, fp);
    }
}

/*
 * Indented fprintf. Print output given by <fmt> and the following parameters,
 * preceded by <indent> levels of indent, to <fp>. Returns the number of
 * characters printed (just like fprintf). */
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
 * Dump <size> bytes from <data> into a new string buffer, using indent
 * <indent>. The address of the string buffer is returned through <str>, its
 * length is this function's return value. Afterwards, the caller is
 * responsible for the string buffer and should free it when it is no longer
 * needed.
 */
int ihexstr(char **str, int indent, const char *data, size_t size)
{
    Buffer *output = bufCreate();
    size_t i, end, begin = 0;

    while (begin < size) {
        bufAddF(output, "%*s", 2 * indent, "");

        end = MIN(begin + HEXDUMP_BYTES_PER_LINE, size);

        bufAddF(output, "%06lX  ", begin);

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
 * Dump <size> bytes from <data> as a hexdump to <fp>, with indent level
 * <indent>.
 */
void ihexdump(FILE *fp, int indent, const char *data, size_t size)
{
    unsigned int i, offset = 0;
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
 * Dump the file descriptors up to <nfds> in <fds> to <fp>, preceded by
 * <intro> (if <intro> is not NULL).
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
void *memdup(const void *src, size_t size)
{
    char *dupe = malloc(size);

    memcpy(dupe, src, size);

    return dupe;
}

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00
 * UTC) as a double.
 */
double dnow(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (double) tv.tv_sec + (double) tv.tv_usec / 1000000L;
}

/*
 * Format the timestamp given by <sec> and <nsec> to a string, using the
 * strftime-compatible format <fmt> and timezone <tz>. If <tz> is NULL, local
 * time (according to the TZ environment variable) is used.
 *
 * This function supports an extension to the %S format specifier: an optional
 * single digit between the '%' and 'S' gives the number of sub-second digits
 * to add to the seconds value. Leaving out the digit altogether reverts back
 * to the default strftime seconds value; giving it as 0 rounds it to the
 * nearest second, based on the value of <nsec>.
 *
 * Returns a statically allocated string that the caller isn't supposed to
 * modify. If you need a string to call your own, use strdup() or call
 * t_format() below.
 */
const char *t_format_c(int32_t sec, int32_t nsec,
        const char *tz, const char *fmt)
{
    int precision;
    char sec_string[13]; // "00.987654321\0"

    static regex_t *regex = NULL;
    const size_t nmatch = 2;
    regmatch_t pmatch[nmatch];

    if (regex == NULL) {
        regex = calloc(1, sizeof(*regex));
        regcomp(regex, "%([0-9])S", REG_EXTENDED);
    }

    char *saved_tz = NULL;

    if (tz != NULL) {
        saved_tz = getenv("TZ");
        setenv("TZ", tz, true);
        tzset();
    }

    struct tm tm = { .tm_isdst = -1 };
    time_t sec_copy = sec;

    localtime_r(&sec_copy, &tm);

    char *cur_fmt, *new_fmt = NULL;
    int field_width;

    static char *result_str = NULL;
    static int  result_size = 0;

    if (asprintf(&cur_fmt, "X%s", fmt) != -1) {
        while (regexec(regex, cur_fmt, nmatch, pmatch, 0) == 0) {
            sscanf(cur_fmt + pmatch[1].rm_so, "%d", &precision);

            if (precision == 0) {
                field_width = 2;
            }
            else {
                field_width = 3 + precision;
            }

            snprintf(sec_string, sizeof(sec_string), "%0*.*f",
                    field_width, precision,
                    tm.tm_sec + nsec / 1000000000.0);

            if (asprintf(&new_fmt, "%.*s%s%s",
                    pmatch[0].rm_so, cur_fmt,
                    sec_string,
                    cur_fmt + pmatch[0].rm_eo) == -1) {
                break;
            }

            cur_fmt = strdup(new_fmt);

            free(new_fmt);
        }

        if (result_str == NULL) {
            result_size = 8;
            result_str = calloc(1, result_size);
        }

        while (strftime(result_str, result_size, cur_fmt, &tm) == 0) {
            result_size *= 2;
            result_str = realloc(result_str, result_size);
        }

        free(cur_fmt);
    }

    if (tz != NULL) {
        if (saved_tz == NULL) {
            unsetenv("TZ");
        }
        else {
            setenv("TZ", saved_tz, 1);
        }
        tzset();
    }

    return result_str ? result_str + 1 : NULL;
}

/*
 * Identical to t_format_c() above, but returns a dynamically allocated
 * string that you should free() when you're done with it.
 */
char *t_format(int32_t sec, int32_t nsec, const char *tz, const char *fmt)
{
    return strdup(t_format_c(sec, nsec, tz, fmt));
}

/*
 * Pack <data>, whose size is <size>, into <*dest> which has
 * <*remaining> bytes remaining. Update <*dest> and <*remaining> to
 * the new situation, *even* if there is not enough room (although
 * nothing is actually written in that case).
 */
static void pack(void *data, size_t size, char **dest, ssize_t *remaining)
{
    if (*remaining >= (ssize_t) size) memcpy(*dest, data, size);

    *dest += size;
    *remaining -= size;
}

/*
 * Pack data from <ap> into <str>, which has size <size>.
 *
 * The pack functions take type/value pairs (closed by END) to specify what to
 * pack into the string. The types, values and packed data are as follows:
 *
 * type         value           packs
 * ----         -----           -----
 * PACK_INT8    int             1 byte int
 * PACK_INT16   int             2 byte int
 * PACK_INT32   int             4 byte int
 * PACK_INT64   uint64_t        8 byte int
 * PACK_FLOAT   double          4 byte float
 * PACK_DOUBLE  double          8 byte double
 * PACK_STRING  char *          4-byte length (from strlen),
 *                              followed by as many bytes.
 * PACK_DATA    char *, uint    4-byte length (as given),
 *                              followed by as many bytes.
 * PACK_RAW     char *, uint    Raw bytes using length as given.
 *
 * All ints (including the lengths) are packed with big-endian byte order.
 *
 * All pack functions return the number of bytes necessary to pack the given
 * data, which may be more than <size>. In that case they are telling you that
 * more bytes were needed than you gave them, and you should call the function
 * again with a bigger data buffer. It will, however, never write more than
 * <size> bytes into <str>.
 */
int vstrpack(char *str, size_t size, va_list ap)
{
    int type;
    ssize_t ssize = size;

    char *p = str;

    while ((type = va_arg(ap, int)) != END) {
        switch(type) {
        case PACK_INT8:
            {
                uint8_t u8 = va_arg(ap, unsigned int);
                pack(&u8, sizeof(u8), &p, &ssize);
            }
            break;
        case PACK_INT16:
            {
                uint16_t u16 = htobe16(va_arg(ap, unsigned int));
                pack(&u16, sizeof(u16), &p, &ssize);
            }
            break;
        case PACK_INT32:
            {
                uint32_t u32 = htobe32(va_arg(ap, unsigned int));
                pack(&u32, sizeof(u32), &p, &ssize);
            }
            break;
        case PACK_INT64:
            {
                uint64_t u64 = htobe64(va_arg(ap, uint64_t));
                pack(&u64, sizeof(u64), &p, &ssize);
            }
            break;
        case PACK_FLOAT:
            {
                float f32 = va_arg(ap, double);
                char *cp = ((char *) &f32);
                uint32_t u32 = *((uint32_t *) cp);

                u32 = htobe32(u32);

                pack(&u32, sizeof(u32), &p, &ssize);
            }
            break;
        case PACK_DOUBLE:
            {
                double f64 = va_arg(ap, double);
                char *cp = ((char *) &f64);
                uint64_t u64 = *((uint64_t *) cp);

                u64 = htobe64(u64);

                pack(&u64, sizeof(u64), &p, &ssize);
            }
            break;
        case PACK_STRING:
            {
                char *s = va_arg(ap, char *);
                uint32_t len_h  = strlen(s);
                uint32_t len_be = htobe32(len_h);

                pack(&len_be, sizeof(len_be), &p, &ssize);
                pack(s, len_h, &p, &ssize);
            }
            break;
        case PACK_DATA:
            {
                char *s = va_arg(ap, char *);
                uint32_t len_h  = va_arg(ap, unsigned int);
                uint32_t len_be = htobe32(len_h);

                pack(&len_be, sizeof(len_be), &p, &ssize);
                pack(s, len_h, &p, &ssize);
            }
            break;
        case PACK_RAW:
            {
                char *s = va_arg(ap, char *);
                uint32_t len_h  = va_arg(ap, unsigned int);

                pack(s, len_h, &p, &ssize);
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
int strpack(char *str, size_t size, ...)
{
    int r;
    va_list ap;

    va_start(ap, size);
    r = vstrpack(str, size, ap);
    va_end(ap);

    return r;
}

/*
 * This function does the same as vstrpack, but it will allocate a
 * sufficiently sized data buffer for you and return it through <str>.
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
 * This function does the same as strpack, but it will allocate a sufficiently
 * sized data buffer for you and return it through <str>.
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
 * The unpack functions take type/pointer pairs (closed by END) where data is
 * extracted from <str> and put into the addresses that the pointers point to.
 * The types, pointers and unpacked data are as follows:
 *
 * type         pointer         unpacks
 * ----         -------         -----
 * PACK_INT8    uint8_t *       1 byte int
 * PACK_INT16   uint16_t *      2 byte int
 * PACK_INT32   uint32_t *      4 byte int
 * PACK_INT64   uint64_t *      8 byte int
 * PACK_FLOAT   float *         4 byte float
 * PACK_DOUBLE  double *        8 byte double
 * PACK_STRING  char **         4-byte length, followed by as many bytes.
 * PACK_DATA    char **, uint * 4-byte length, followed by as many bytes.
 * PACK_RAW     char *, uint    As many raw bytes as given.
 *
 * Note: PACK_STRING and PACK_DATA allocate space to put the data in, and it
 * is the caller's responsibility to free that space again. PACK_STRING
 * creates a null-terminated string. PACK_DATA requires an additional int *
 * where it writes the length of the allocated data.
 *
 * This function returns the minimum number of bytes necessary to unpack the
 * given fields, which may be more than <size>. "Minimum" in this case means
 * assuming all PACK_STRING and PACK_DATA fields have length 0.
 */
int vstrunpack(const char *str, size_t size, va_list ap)
{
    int type;
    ssize_t ssize = size;

    const char *ptr = str;

    while ((type = va_arg(ap, int)) != END) {
        switch(type) {
        case PACK_INT8:
            {
                uint8_t *u8 = va_arg(ap, uint8_t *);
                if (u8 != NULL && ssize >= (ssize_t) sizeof(uint8_t)) {
                    *u8 = *((uint8_t *) ptr);
                }
                ptr += sizeof(uint8_t);
                ssize -= sizeof(uint8_t);
            }
            break;
        case PACK_INT16:
            {
                uint16_t *u16 = va_arg(ap, uint16_t *);
                if (u16 != NULL && ssize >= (ssize_t) sizeof(uint16_t)) {
                    *u16 = be16toh(*((uint16_t *) ptr));
                }
                ptr += sizeof(uint16_t);
                ssize -= sizeof(uint16_t);
            }
            break;
        case PACK_INT32:
            {
                uint32_t *u32 = va_arg(ap, uint32_t *);
                if (u32 != NULL && ssize >= (ssize_t) sizeof(uint32_t)) {
                    *u32 = be32toh(*((uint32_t *) ptr));
                }
                ptr += sizeof(uint32_t);
                ssize -= sizeof(uint32_t);
            }
            break;
        case PACK_INT64:
            {
                uint64_t *u64 = va_arg(ap, uint64_t *);
                if (u64 != NULL && ssize >= (ssize_t) sizeof(uint64_t)) {
                    *u64 = be64toh(*((uint64_t *) ptr));
                }
                ptr += sizeof(uint64_t);
                ssize -= sizeof(uint64_t);
            }
            break;
        case PACK_FLOAT:
            {
                float *f32 = va_arg(ap, float *);
                if (f32 != NULL && ssize >= (ssize_t) sizeof(float)) {
                    uint32_t u32 = *((uint32_t *) ptr);
                    u32 = be32toh(u32);
                    char *cp = (char *) &u32;
                    *f32 = *((float *) cp);
                }
                ptr += sizeof(float);
                ssize -= sizeof(float);
            }
            break;
        case PACK_DOUBLE:
            {
                double *f64 = va_arg(ap, double *);
                if (f64 != NULL && ssize >= (ssize_t) sizeof(double)) {
                    uint64_t u64 = *((uint64_t *) ptr);
                    u64 = be64toh(u64);
                    char *cp = (char *) &u64;
                    *f64 = *((double *) cp);
                }
                ptr += sizeof(double);
                ssize -= sizeof(double);
            }
            break;
        case PACK_STRING:
            {
                int len = 0;
                char **strpp = va_arg(ap, char **);

                if (ssize >= (ssize_t) sizeof(uint32_t)) {
                    len = be32toh(*((uint32_t *) ptr));
                }

                ptr += sizeof(uint32_t);
                ssize -= sizeof(uint32_t);

                if (strpp != NULL && ssize >= len) *strpp = strndup(ptr, len);
                ptr += len;
                ssize -= len;
            }
            break;
        case PACK_DATA:
            {
                char **strpp = va_arg(ap, char **);
                uint32_t len = 0, *lenp = va_arg(ap, uint32_t *);

                if (ssize >= (ssize_t) sizeof(uint32_t)) {
                    len = be32toh(*((uint32_t *) ptr));
                }

                ptr += sizeof(uint32_t);
                ssize -= sizeof(uint32_t);

                if (lenp != NULL) {
                    *lenp = len;
                }

                if (strpp != NULL && ssize >= len) {
                    *strpp = malloc(len);
                    memcpy(*strpp, ptr, len);
                }

                ptr += len;
                ssize -= len;
            }
            break;
        case PACK_RAW:
            {
                char *strp = va_arg(ap, char *);
                int len = va_arg(ap, unsigned int);

                if (strp != NULL && ssize >= len) {
                    memcpy(strp, ptr, len);
                }

                ptr  += len;
                ssize -= len;
            }
            break;
        }
    }

    return ptr - str;
}

/*
 * Unpack data from <str> (which has size <size>) into the pointers following
 * "size".
 */
int strunpack(const char *str, size_t size, ...)
{
    int r;
    va_list ap;

    va_start(ap, size);
    r = vstrunpack(str, size, ap);
    va_end(ap);

    return r;
}

/*
 * Expand environment variables in <text> and return the result. Non-existing
 * variables are replaced with empty strings. Any dollar sign followed by 0 or
 * more letters, digits or underscores is assumed to be an environment variable
 * (which is probably more than your shell).
 */
char *env_expand(const char *text)
{
    const char *p;

    Buffer *varname = bufCreate();
    Buffer *result = bufCreate();

    for (p = text; *p != '\0'; p++) {
        if (*p == '$') {
            bufClear(varname);
            for (p = p+1; isalnum(*p) || *p == '_'; p++) {
                bufAddC(varname, *p);
            }

            char *value = getenv(bufGet(varname));

            if (value != NULL) {
                bufAddS(result, value);
            }

            p--;
        }
        else {
            bufAddC(result, *p);
        }
    }

    bufDestroy(varname);

    return bufFinish(result);
}

/*
 * From:
 * https://github.com/elliotchance/c2go/blob/4ecf1a1adaafea18d7f23ab5632e67357fb2f56e/tests/tests.h
 *
 * This will strictly not handle any input value that is infinite or NaN.
 * It will always return false, even if the values are exactly equal. This is to
 * force you to use the correct matcher (ie. is_inf()) instead of relying on
 * comparisons which might not work.
 */
int close_to(double actual, double expected)
{
    const int bits = 48;

    // We do not handle infinities or NaN.
    if (isinf(actual) || isinf(expected) || isnan(actual) || isnan(expected)) {
        return 0;
    }

    // If we expect zero (a common case) we have a fixed epsilon from actual. If
    // allowed to continue the epsilon calculated would be zero and we would be
    // doing an exact match which is what we want to avoid.
    if (expected == 0.0) {
        return fabs(actual) < (1 / pow(2, bits));
    }

    // The epsilon is calculated based on significant bits of the actual value.
    // The amount of bits used depends on the original size of the float (in
    // terms of bits) subtracting a few to allow for very slight rounding
    // errors.
    double epsilon = fabs(expected / pow(2, bits));

    // The numbers are considered equal if the absolute difference between them
    // is less than the relative epsilon.
    return fabs(actual - expected) <= epsilon;
}

/*
 * Test <val>. If it is FALSE, print that fact, along with the textual
 * representation of <val> which is in <str>, the file and line on which the
 * error occurred in <file> and <line> to stderr, and increase <*errors> by 1.
 * Called by the make_sure_that() macro, less useful on its own.
 */
int _make_sure_that(const char *file, int line,
                     int *errors, const char *str, int val)
{
    if (!val) {
        fprintf(stderr, "%s:%d: Expression \"%s\" failed\n", file, line, str);

        (*errors)++;

        return 1;
    }

    return 0;
}

/*
 * Check that strings <s1> and <s2> are identical. Complain and return 1 if
 * they're not, otherwise return 0.
 */
int _check_string(const char *file, int line,
        int *errors, const char *s1, const char *s2)
{
    if (strcmp(s1, s2) != 0) {
        fprintf(stderr, "%s:%d: String does not match expectation.\n",
                file, line);
        fprintf(stderr, "Expected:\n");
        fprintf(stderr, "<%s>\n", s1);
        fprintf(stderr, "Actual:\n");
        fprintf(stderr, "<%s>\n", s2);

        (*errors)++;

        return 1;
    }

    return 0;
}

/*
 * Increase the size of <str> (whose current size is pointed to by <size>) to
 * <new_size>, and return the (possibly changed) address of the embiggened
 * string, while also updating <size> to the new size.
 */
static char *embiggen(char *str, size_t *size, size_t new_size)
{
    assert(new_size >= *size);

    str = realloc(str, new_size);
    memset(str + *size, 0, new_size - *size);
    *size = new_size;

    return str;
}

/*
 * Use the iconv converter <cd> to convert the character string pointed to by
 * <input>, with length <input_len>, and return a pointer to the result. If
 * <output_len> is not NULL, the length of the returned string is written
 * there. <cd> is an "iconv_t" as returned by the iconv_open() function. This
 * is basically a wrapper around iconv() to make it a little more convenient
 * to use.
 *
 * The returned string will always have a terminating null byte, but
 * note that some character encodings supported by iconv may allow an embedded
 * null byte in a string. In those cases you should use <output_len> to get
 * the "real" length of the returned string. If the conversion could not be
 * performed because of an encoding error in <input>, NULL is returned and
 * errno is set to EINVAL.
 */
const char *convert_charset(iconv_t cd, const char *input, size_t input_len,
                            size_t *output_len)
{
    static char *out = NULL;
    static size_t out_size = 4;

    size_t inbytesleft;
    size_t outbytesleft;

    if (out == NULL) out = calloc(1, out_size);

    while (1) {
        inbytesleft = input_len;
        outbytesleft = out_size;

        char *in_ptr = (char *) input;
        char *out_ptr = out;

        size_t r = iconv(cd, &in_ptr, &inbytesleft, &out_ptr, &outbytesleft);

        if (r == (size_t) -1) {
            if (errno == E2BIG) {
                out = embiggen(out, &out_size, 2 * out_size);
            }
            else {
                return NULL;
            }
        }
        else {
            if (outbytesleft == 0) {
                outbytesleft += out_size;
                out = embiggen(out, &out_size, 2 * out_size);
            }
            break;
        }
    }

    if (output_len != NULL) *output_len = out_size - outbytesleft;

    return out;
}
