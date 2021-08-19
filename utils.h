#ifndef LIBJVS_UTILS_H
#define LIBJVS_UTILS_H

/*
 * utils.h: Various utility functions.
 *
 * utils.h is part of libjvs.
 *
 * Copyright:   (c) 2012-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: utils.h 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <iconv.h>

#ifndef __WIN32
#include <sys/select.h>
#else
#include <winsock.h>
#endif

#define hexdump(fp, data, size) ihexdump(fp, 0, data, size)
#define hexstr(spp, data, size) ihexstr(spp, 0, data, size)

#define make_sure_that(expr) \
    _make_sure_that(__FILE__, __LINE__, &errors, #expr, (expr))
#define check_string(s1, s2) \
    _check_string(__FILE__, __LINE__, &errors, s1, s2);
#define check_timespec(t, sec, nsec) \
    _check_timespec(__FILE__, __LINE__, #t, &errors, t, sec, nsec)
#define check_timeval(t, sec, usec) \
    _check_timeval(__FILE__, __LINE__, #t, &errors, t, sec, usec)

/*
 * "Packable" types.
 */
enum {
    PACK_INT8,
    PACK_INT16,
    PACK_INT32,
    PACK_INT64,
    PACK_FLOAT,
    PACK_DOUBLE,
    PACK_STRING,
    PACK_DATA,
    PACK_RAW
};

/*
 * Return the current stack depth.
 */
int stackdepth(void);

/*
 * Set the indent string used by findent and ifprintf.
 */
void set_indent_string(const char *str);

/*
 * Output <level> levels of indent to <fp>.
 */
void findent(FILE *fp, int level);

/*
 * Indented fprintf. Print output given by <fmt> and the following parameters,
 * preceded by <indent> levels of indent, to <fp>. Returns the number of
 * characters printed (just like fprintf). */
int ifprintf(FILE *fp, int indent, const char *fmt, ...);

/*
 * Dump <size> bytes from <data> into a new string buffer, using indent
 * <indent>. The address of the string buffer is returned through <str>, its
 * length is this function's return value. Afterwards, the caller is
 * responsible for the string buffer and should free it when it is no longer
 * needed.
 */
int ihexstr(char **str, int indent, const char *data, size_t size);

/*
 * Dump <size> bytes from <data> as a hexdump to <fp>, with indent level
 * <indent>.
 */
void ihexdump(FILE *fp, int indent, const char *data, size_t size);

/*
 * Dump the file descriptors up to <nfds> in <fds> to <fp>, preceded by
 * <intro> (if <intro> is not NULL).
 */
void dumpfds(FILE *fp, const char *intro, int nfds, fd_set *fds);

/*
 * Duplicate <size> bytes starting at <src> and return a pointer to the
 * duplicate.
 */
void *memdup(const void *src, size_t size);

/*
 * Convert the double timestamp <t> to a struct timeval in <tv>.
 */
void dtotv(double t, struct timeval *tv);

/*
 * Return the timestamp in <tv> as a double.
 */
double tvtod(const struct timeval *tv);

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00
 * UTC) as a double.
 */
double dnow(void);

/*
 * Return the current time as a struct timespec.
 */
const struct timespec *tsnow(void);

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
        const char *tz, const char *fmt);

/*
 * Identical to t_format_c() above, but returns a dynamically allocated
 * string that you should free() when you're done with it.
 */
char *t_format(int32_t sec, int32_t nsec, const char *tz, const char *fmt);

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
int vstrpack(char *str, size_t size, va_list ap);

/*
 * Pack data into <str>, using a variable number of arguments.
 */
int strpack(char *str, size_t size, ...);

/*
 * This function does the same as vstrpack, but it will allocate a
 * sufficiently sized data buffer for you and return it through <str>.
 */
int vastrpack(char **str, va_list ap);

/*
 * This function does the same as strpack, but it will allocate a sufficiently
 * sized data buffer for you and return it through <str>.
 */
int astrpack(char **str, ...);

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
int vstrunpack(const char *str, size_t size, va_list ap);

/*
 * Unpack data from <str> (which has size <size>) into the pointers following
 * "size".
 */
int strunpack(const char *str, size_t size, ...);

/*
 * Expand environment variables in <text> and return the result. Non-existing
 * variables are replaced with empty strings. Any dollar sign followed by 0 or
 * more letters, digits or underscores is assumed to be an environment variable
 * (which is probably more than your shell).
 */
char *env_expand(const char *text);

/*
 * From:
 * https://github.com/elliotchance/c2go/blob/4ecf1a1adaafea18d7f23ab5632e67357fb2f56e/tests/tests.h
 *
 * This will strictly not handle any input value that is infinite or NaN.
 * It will always return false, even if the values are exactly equal. This is to
 * force you to use the correct matcher (ie. is_inf()) instead of relying on
 * comparisons which might not work.
 */
int close_to(double actual, double expected);

/*
 * Test <val>. If it is FALSE, print that fact, along with the textual
 * representation of <val> which is in <str>, the file and line on which the
 * error occurred in <file> and <line> to stderr, and increase <*errors> by 1.
 * Called by the make_sure_that() macro, less useful on its own.
 */
int _make_sure_that(const char *file, int line,
                     int *errors, const char *str, int val);

/*
 * Check that strings <s1> and <s2> are identical. Complain and return 1 if
 * they're not, otherwise return 0.
 */
int _check_string(const char *file, int line,
        int *errors, const char *s1, const char *s2);

/*
 * Check that the timespec in <t> contains <sec> and <nsec>. If not, print a
 * message to that effect on stderr (using <file> and <line>, which should
 * contain the source file and line where this function was called) and
 * increment the error counter pointed to by <errors>. This function is used
 * in test code, and should be called using the check_timespec macro.
 */
void _check_timespec(const char *file, int line,
                     const char *name, int *errors,
                     struct timespec t, long sec, long nsec);

/*
 * Check that the timeval in <t> contains <sec> and <nsec>. If not, print a
 * message top that effect on stderr (using <file> and <line>, which should
 * contain the source file and line where this function was called) and
 * increment the error counter pointed to by <errors>. This function is used
 * in test code, and should be called using the check_timeval macro.
 */
void _check_timeval(const char *file, int line,
                    const char *name, int *errors,
                    struct timeval t, long sec, long usec);

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
                            size_t *output_len);

#ifdef __cplusplus
}
#endif

#endif
