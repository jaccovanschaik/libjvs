#ifndef TIME_H
#define TIME_H

/*
 * timespec.h: calculations with struct timespec's.
 *
 * Copyright: (c) 2020-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-10-22
 * Version:   $Id: timespec.h 454 2022-02-12 23:39:20Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

/*
 * Return a normalized version of <ts>, where tv_nsec lies in [0, 10^9> and
 * tv_sec is adjusted accordingly.
 */
struct timespec tsNormalized(struct timespec ts);

/*
 * Normalize <ts>: make sure ts->tv_nsec lies in [0, 10^9> and adjust
 * ts->tv_sec accordingly.
 */
void tsNormalize(struct timespec *ts);

/*
 * Return a pointer to a new timespec, filled with the values in <sec> and
 * <nsec>, and normalized.
 */
struct timespec *tsCreate(time_t sec, long nsec);

/*
 * Return a timespec set to the values in <sec> and <nsec>, and normalized.
 */
struct timespec tsMake(time_t sec, long nsec);

/*
 * Return the current time as a struct timespec.
 */
struct timespec tsNow(void);

/*
 * Compare <t1> and <t0>. Returns -1 if <t1> is less than <t0>, 1 if <t1> is
 * greater than <t0> or 0 if they are equal.
 */
int tsCompare(struct timespec t1, struct timespec t0);

/*
 * Return the difference between t1 and t0 (i.e. t1 - t0) as a double.
 */
double tsDelta(struct timespec t1, struct timespec t0);

/*
 * Subtract <seconds> from <ts> and return the result as a new timespec.
 */
struct timespec tsDec(struct timespec ts, double seconds);

/*
 * Add <seconds> to <ts> and return the result as a new timespec.
 */
struct timespec tsInc(struct timespec ts, double seconds);

/*
 * Return a timespec derived from the double time value in <t>.
 */
struct timespec tsFromDouble(double t);

/*
 * Return a double precision time value derived from timespec.
 */
double tsToDouble(struct timespec ts);

/*
 * Return a timespec struct derived from timeval <tv>.
 */
struct timespec tsFromTimeval(struct timeval tv);

/*
 * Format the timestamp given by <ts> to a string, using the
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
 * tsFormat() below.
 */
const char *tsFormatC(const struct timespec ts, const char *tz,
                      const char *fmt);

/*
 * Identical to tsFormatC() above, but returns a dynamically allocated string
 * that you should free() when you're done with it.
 */
char *tsFormat(const struct timespec ts, const char *tz, const char *fmt);

#ifdef __cplusplus
}
#endif

#endif
