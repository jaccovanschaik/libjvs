/*
 * utils.c: Description
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdarg.h>

#include "utils.h"

/*
 * Output <level> levels of indent to <fp>.
 */
void findent(FILE *fp, int level)
{
    int i;

    for (i = 0; i < level; i++) {
        fputs("    ", fp);
    }
}

/*
 * Print output given by <fmt> and the following parameters, preceded by
 * <indent> levels of indent, to <fp>. Returns the number of characters
 * printed (just like fprintf).
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

