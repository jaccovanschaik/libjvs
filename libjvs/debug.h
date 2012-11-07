/*
 * Functions to assist debugging.
 *
 * Copyright: (c) 2004 Jacco van Schaik (jacco@frontier.nl)
 * Version:   $Id: debug.h 243 2007-06-24 13:33:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdarg.h>

#ifndef __GNUC__
static char *__func__ = NULL;
#endif

/* call the functions below using these macros. They set the current position in
 * the source code and then call the associated function. */

#define dbgTrace  (_dbgSetPos(__FILE__, __LINE__, __func__), _dbgTrace)
#define dbgAbort  (_dbgSetPos(__FILE__, __LINE__, __func__), _dbgAbort)
#define dbgPrint  (_dbgSetPos(__FILE__, __LINE__, __func__), _dbgPrint)
#define dbgAssert (_dbgSetPos(__FILE__, __LINE__, __func__), _dbgAssert)
#define dbgError  (_dbgSetPos(__FILE__, __LINE__, __func__), _dbgError)

/* Set the current position for subsequent error messages. */
void _dbgSetPos(char *file, int line, const char *func);

/*
 * Print the given debugging message, indented to the current stack depth/
 */
void _dbgTrace(FILE *fp, const char *fmt, ...)
        __attribute__ ((format (printf, 2, 3)));

/* Call abort(), preceded with the given message. */
void _dbgAbort(FILE *fp, const char *fmt, ...)
        __attribute__ ((format (printf, 2, 3)));

/* Print the given debugging message on <fp>. */
void _dbgPrint(FILE *fp, const char *fmt, ...)
        __attribute__ ((format (printf, 2, 3)));

/* Check <cond> and, if false, print the given message and call abort(). */
void _dbgAssert(FILE *fp, int cond, const char *fmt, ...)
        __attribute__ ((format (printf, 3, 4)));

/* Print the given debugging message on <fp>, followed by the error
 * message associated with the current value of errno. */
void _dbgError(FILE *fp, const char *fmt, ...)
        __attribute__ ((format (printf, 2, 3)));

#endif
