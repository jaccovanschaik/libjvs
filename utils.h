#ifndef LIBJVS_UTILS_H
#define LIBJVS_UTILS_H

/*
 * utils.h: Various utility functions.
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "defs.h"

#include <stdio.h>
#include <stdarg.h>
#include <sys/select.h>

#define hexdump(fp, data, size) ihexdump(fp, 0, data, size)
#define hexstr(spp, data, size) ihexstr(spp, 0, data, size)

#define make_sure_that(expr) _make_sure_that(__FILE__, __LINE__, &errors, #expr, (expr))

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
 * Output <level> levels of indent to <fp>.
 */
void findent(FILE *fp, int level);

/*
 * Indented fprintf. Print output given by <fmt> and the following parameters, preceded by <indent>
 * levels of indent, to <fp>. Returns the number of characters printed (just like fprintf).
 */
int ifprintf(FILE *fp, int indent, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

/*
 * Dump <size> bytes from <data> into a new string buffer, using indent <indent>. The address of the
 * string buffer is returned through <str>, its length is this function's return value. Afterwards,
 * the caller is responsible for the string buffer and should free it when it is no longer needed.
 */
int ihexstr(char **str, int indent, const char *data, int size);

/*
 * Dump <size> bytes from <data> as a hexdump to <fp>, with indent level <indent>.
 */
void ihexdump(FILE *fp, int indent, const char *data, int size);

/*
 * Dump the file descriptors up to <nfds> in <fds> to <fp>, preceded by <intro> (if <intro> is not
 * NULL).
 */
void dumpfds(FILE *fp, const char *intro, int nfds, fd_set *fds);

/*
 * Duplicate <size> bytes starting at <src> and return a pointer to the
 * duplicate.
 */
void *memdup(const void *src, unsigned int size);

/*
 * Convert the double timestamp <t> to a struct timeval in <tv>.
 */
void dtotv(double t, struct timeval *tv);

/*
 * Return the timestamp in <tv> as a double.
 */
double tvtod(const struct timeval *tv);

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00 UTC) as a double.
 */
double nowd(void);

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
int vstrpack(char *str, int size, va_list ap);

/*
 * Pack data into <str>, using a variable number of arguments.
 */
int strpack(char *str, int size, ...);

/*
 * This function does the same as vstrpack, but it will allocate a sufficiently sized data buffer
 * for you and return it through <str>.
 */
int vastrpack(char **str, va_list ap);

/*
 * This function does the same as strpack, but it will allocate a sufficiently sized data buffer for
 * you and return it through <str>.
 */
int astrpack(char **str, ...);

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
int vstrunpack(const char *str, int size, va_list ap);

/*
 * Unpack data from <str> (which has size <size>) into the pointers following "size".
 */
int strunpack(const char *str, int size, ...);

/*
 * Test <val>. If it is FALSE, print that fact, along with the textual representation of <val> which
 * is in <str>, the file and line on which the error occurred in <file> and <line> to stderr, and
 * increase <*errors> by 1. Called by the make_sure_that() macro, less useful on its own.
 */
void _make_sure_that(const char *file, int line, int *errors, const char *str, int val);

#endif
