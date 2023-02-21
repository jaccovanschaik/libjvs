#ifndef LIBJVS_DEBUG_H
#define LIBJVS_DEBUG_H

/*
 * debug.h: functions to assist debugging.
 *
 * debug.h is part of libjvs.
 *
 * Copyright:   (c) 2004-2023 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: debug.h 475 2023-02-21 08:08:11Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

#ifndef __GNUC__
static char *__func__ = NULL;
#endif

/* call the functions below using these macros. They set the current position in
 * the source code and then call the associated function. */

#define dbgTrace(fp, ...)  \
    _dbgTrace(fp,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define dbgAbort(fp, ...)  \
    _dbgAbort(fp,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define dbgPrint(fp, ...)  \
    _dbgPrint(fp,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define dbgError(fp, ...)  \
    _dbgError(fp,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define dbgAssert(fp, cond, ...) \
    _dbgAssert(fp, cond, __FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * Print the given debugging message, indented to the current stack depth/
 */
__attribute__((format (printf, 5, 6)))
void _dbgTrace(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...);

/*
 * Call abort(), preceded with the given message.
 */
__attribute__ ((noreturn))
__attribute__((format (printf, 5, 6)))
void _dbgAbort(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...);

/*
 * Print the given debugging message on <fp>.
 */
__attribute__((format (printf, 5, 6)))
void _dbgPrint(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...);

/*
 * Check <cond> and, if false, print the given message and call abort().
 */
__attribute__((format (printf, 6, 7)))
void _dbgAssert(FILE *fp, int cond,
        const char *file, int line, const char *func,
        const char *fmt, ...);

/*
 * Print the given debugging message on <fp>, followed by the error
 * message associated with the current value of errno.
 */
__attribute__((format (printf, 5, 6)))
void _dbgError(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
