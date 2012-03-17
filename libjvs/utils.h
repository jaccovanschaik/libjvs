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

#endif
