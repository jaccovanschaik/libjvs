#ifndef UTILS_H
#define UTILS_H

/*
 * utils.h: Description
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>

enum {
    PACK_INT8,
    PACK_UINT8,
    PACK_INT16,
    PACK_UINT16,
    PACK_INT32,
    PACK_UINT32,
    PACK_INT64,
    PACK_UINT64,
    PACK_FLOAT,
    PACK_DOUBLE,
    PACK_STRING,
    PACK_DATA
};

/*
 * Output <level> levels of indent to <fp>.
 */
void findent(FILE *fp, int level);

/*
 * Print output given by <fmt> and the following parameters, preceded by
 * <indent> levels of indent, to <fp>. Returns the number of characters
 * printed (just like fprintf).
 */
int ifprintf(FILE *fp, int indent, const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)));

/*
 * Dump <size> bytes from <data> as a hexdump to <fp>.
 */
void hexdump(FILE *fp, const char *data, int size);

int vstrpack(char *str, int size, va_list ap);

int strpack(char *str, int size, ...);

int vstrunpack(char *str, int size, va_list ap);

int strunpack(char *str, int size, ...);

#endif
