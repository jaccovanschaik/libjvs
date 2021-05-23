#ifndef TIME_H
#define TIME_H

/*
 * timespec.h: calculations with struct timespec's.
 *
 * Copyright: (c) 2020 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-10-22
 * Version:   $Id: timespec.h 417 2021-05-23 12:34:55Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <sys/time.h>

/*
 * Normalize <t>: make sure t->tv_nsec lies in [0, 10^9> and adjust t->tv_sec
 * accordingly.
 */
void tsNormalize(struct timespec *t);

/*
 * Return a pointer to a new timespec, filled with the values in <sec> and
 * <nsec>, and normalized.
 */
struct timespec *tsCreate(long sec, long nsec);

/*
 * Return a timespec set to the values in <sec> and <nsec>, and normalized.
 */
struct timespec tsMake(long sec, long nsec);

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
double tsDiff(struct timespec t1, struct timespec t0);

/*
 * Subtract <seconds> from <t> and return the result as a new timespec.
 */
struct timespec tsSub(struct timespec t, double seconds);

/*
 * Add <seconds> to <t> and return the result as a new timespec.
 */
struct timespec tsAdd(struct timespec t, double seconds);

/*
 * Return a timespec derived from the double time value in <t>.
 */
struct timespec tsFromDouble(double t);

/*
 * Return a double precision time value derived from timespec.
 */
double tsToDouble(struct timespec t);

/*
 * Return a timeval struct derived from timespec <t>.
 */
struct timeval tsToTimeval(struct timespec t);

/*
 * Return a timespec struct derived from timeval <t>.
 */
struct timespec tsFromTimeval(struct timeval t);

int main(void);

#endif
