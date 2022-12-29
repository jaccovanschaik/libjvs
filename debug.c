/*
 * debug.c: functions to assist debugging.
 *
 * debug.c is part of libjvs.
 *
 * Copyright:   (c) 2004-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: debug.c 472 2022-12-29 20:18:27Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <execinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "debug.h"
#include "utils.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Print the current position.
 */
static void print_position(FILE *fp,
        const char *file, int line, const char *func)
{
   fprintf(fp, "%s:%d: ", file, line);

   if (func) fprintf(fp, "(%s) ", func);
}

/*
 * Print the given debugging message, indented to the current stack depth/
 */
__attribute__((format (printf, 5, 6)))
void _dbgTrace(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...)
{
   va_list ap;

   pthread_mutex_lock(&mutex);

   findent(fp, stackdepth() - 1);

   print_position(fp, file, line, func);

   va_start(ap, fmt);
   vfprintf(fp, fmt, ap);
   va_end(ap);

   fflush(fp);

   pthread_mutex_unlock(&mutex);
}

/*
 * Call abort(), preceded with the given message.
 */
__attribute__ ((noreturn))
__attribute__((format (printf, 5, 6)))
void _dbgAbort(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...)
{
   va_list ap;

   pthread_mutex_lock(&mutex);

   print_position(fp, file, line, func);

   va_start(ap, fmt);
   vfprintf(fp, fmt, ap);
   va_end(ap);

   fflush(fp);

   pthread_mutex_unlock(&mutex);

   abort();
}

/*
 * Print the given debugging message on <fp>.
 */
__attribute__((format (printf, 5, 6)))
void _dbgPrint(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...)
{
   va_list ap;

   pthread_mutex_lock(&mutex);

   print_position(fp, file, line, func);

   va_start(ap, fmt);
   vfprintf(fp, fmt, ap);
   va_end(ap);

   fflush(fp);

   pthread_mutex_unlock(&mutex);
}

/*
 * Check <cond> and, if false, print the given message and call abort().
 */
__attribute__((format (printf, 6, 7)))
void _dbgAssert(FILE *fp, int cond,
        const char *file, int line, const char *func,
        const char *fmt, ...)
{
   if (!cond) {
      va_list ap;

      pthread_mutex_lock(&mutex);

      print_position(fp, file, line, func);

      va_start(ap, fmt);
      vfprintf(fp, fmt, ap);
      va_end(ap);

      fflush(fp);

      pthread_mutex_unlock(&mutex);

      abort();
   }
}

/*
 * Print the given debugging message on <fp>, followed by the error
 * message associated with the current value of errno.
 */
__attribute__((format (printf, 5, 6)))
void _dbgError(FILE *fp, const char *file, int line, const char *func,
        const char *fmt, ...)
{
   va_list ap;

   pthread_mutex_lock(&mutex);

   print_position(fp, file, line, func);

   va_start(ap, fmt);
   vfprintf(fp, fmt, ap);
   va_end(ap);

   fprintf(fp, ": %s\n", strerror(errno));

   fflush(fp);

   pthread_mutex_unlock(&mutex);
}
