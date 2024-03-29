#ifndef TIME_H
#define TIME_H

/*
 * timeval.h: calculations with struct timeval's.
 *
 * Copyright: (c) 2020-2024 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-10-22
 * Version:   $Id: timeval.h 491 2024-02-17 09:55:33Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

/*
 * Return a normalized version of <tv>, where tv_usec lies in [0, 10^9> and
 * tv_sec is adjusted accordingly.
 */
struct timeval tvNormalized(struct timeval tv);

/*
 * Normalize <tv>: make sure tv->tv_usec lies in [0, 10^9> and adjust
 * tv->tv_sec accordingly.
 */
void tvNormalize(struct timeval *tv);

/*
 * Return a pointer to a new timeval, filled with the values in <sec> and
 * <usec>, and normalized.
 */
struct timeval *tvCreate(long sec, long usec);

/*
 * Return a timeval set to the values in <sec> and <usec>, and normalized.
 */
struct timeval tvMake(long sec, long usec);

/*
 * Return the current time as a struct timeval.
 */
struct timeval tvNow(void);

/*
 * Compare <t1> and <t0>. Returns -1 if <t1> is less than <t0>, 1 if <t1> is
 * greater than <t0> or 0 if they are equal.
 */
int tvCompare(struct timeval t1, struct timeval t0);

/*
 * Return the difference between t1 and t0 (i.e. t1 - t0) as a double.
 */
double tvDelta(struct timeval t1, struct timeval t0);

/*
 * Subtract <seconds> from <tv> and return the result as a new timeval.
 */
struct timeval tvDec(struct timeval tv, double seconds);

/*
 * Add <seconds> to <tv> and return the result as a new timeval.
 */
struct timeval tvInc(struct timeval tv, double seconds);

/*
 * Return a timeval derived from the double time value in <t>.
 */
struct timeval tvFromDouble(double t);

/*
 * Return a double precision time value derived from timeval.
 */
double tvToDouble(struct timeval tv);

/*
 * Return a timeval struct derived from timespec <ts>.
 */
struct timeval tvFromTimespec(struct timespec ts);

/*
 * Format the timestamp given by <ts> to a string, using the
 * strftime-compatible format <fmt> and timezone <tz>. If <tz> is NULL, local
 * time (according to the TZ environment variable) is used.
 *
 * This function supports an extension to the %S format specifier: an optional
 * single digit between the '%' and 'S' gives the number of sub-second digits
 * to add to the seconds value. Leaving out the digit altogether reverts back
 * to the default strftime seconds value; giving it as 0 rounds it to the
 * nearest second, based on the value of <usec>.
 *
 * Returns a statically allocated string that the caller isn't supposed to
 * modify. If you need a string to call your own, use strdup() or call
 * tvFormat() below.
 */
const char *tvFormatC(const struct timeval tv, const char *tz,
        const char *fmt);

/*
 * Identical to tvFormatC() above, but returns a dynamically allocated string
 * that you should free() when you're done with it.
 */
char *tvFormat(const struct timeval tv, const char *tz, const char *fmt);

#ifdef __cplusplus
}
#endif

#endif
