/*
 * timespec.c: calculations with struct timespec's.
 *
 * Copyright: (c) 2020 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-10-22
 * Version:   $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "timespec.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#define NSEC_PER_SEC 1000000000L

/*
 * Normalize <t>: make sure t->tv_nsec lies in [0, 10^9> and adjust t->tv_sec
 * accordingly.
 */
void tsNormalize(struct timespec *t)
{
    while (t->tv_nsec < 0) {
        t->tv_sec--;
        t->tv_nsec += NSEC_PER_SEC;
    }

    while (t->tv_nsec >= NSEC_PER_SEC) {
        t->tv_sec++;
        t->tv_nsec -= NSEC_PER_SEC;
    }
}

/*
 * Return a pointer to a new timespec, filled with the values in <sec> and
 * <nsec>, and normalized.
 */
struct timespec *tsCreate(long sec, long nsec)
{
    struct timespec *t = calloc(1, sizeof(*t));

    t->tv_sec  = sec;
    t->tv_nsec = nsec;

    tsNormalize(t);

    return t;
}

/*
 * Return a timespec set to the values in <sec> and <nsec>, and normalized.
 */
struct timespec tsMake(long sec, long nsec)
{
    struct timespec t = {
        .tv_sec = sec,
        .tv_nsec = nsec
    };

    tsNormalize(&t);

    return t;
}

/*
 * Return the current time as a struct timespec.
 */
struct timespec tsNow(void)
{
#if _POSIX_TIMERS > 0
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    struct timespec ts = { .tv_sec = tv.tv_sec, .tv_nsec = tv.tv_usec * 1000 };
#endif

    return ts;
}

/*
 * Compare <t1> and <t0>. Returns -1 if <t1> is less than <t0>, 1 if <t1> is
 * greater than <t0> or 0 if they are equal.
 */
int tsCompare(struct timespec t1, struct timespec t0)
{
    if (t1.tv_sec < t0.tv_sec) {
        return -1;
    }
    else if (t1.tv_sec > t0.tv_sec) {
        return 1;
    }
    else if (t1.tv_nsec < t0.tv_nsec) {
        return -1;
    }
    else if (t1.tv_nsec > t0.tv_nsec) {
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Return the difference between t1 and t0 (i.e. t1 - t0) as a double.
 */
double tsDiff(struct timespec t1, struct timespec t0)
{
    return t1.tv_sec - t0.tv_sec
         + (double) (t1.tv_nsec - t0.tv_nsec) / NSEC_PER_SEC;
}

/*
 * Subtract <seconds> from <t> and return the result as a new timespec.
 */
struct timespec tsSub(struct timespec t, double seconds)
{
    t.tv_sec  -= (long) seconds;
    t.tv_nsec -= (long) (NSEC_PER_SEC * fmod(seconds, 1));

    tsNormalize(&t);

    return t;
}

/*
 * Add <seconds> to <t> and return the result as a new timespec.
 */
struct timespec tsAdd(struct timespec t, double seconds)
{
    t.tv_sec  += (long) seconds;
    t.tv_nsec += (long) (NSEC_PER_SEC * fmod(seconds, 1));

    tsNormalize(&t);

    return t;
}

/*
 * Return a timespec derived from the double time value in <t>.
 */
struct timespec tsFromDouble(double t)
{
    return tsMake((int) t, fmod(NSEC_PER_SEC * t, NSEC_PER_SEC));
}

/*
 * Return a double precision time value derived from timespec.
 */
double tsToDouble(struct timespec t)
{
    return (double) t.tv_sec + (double) t.tv_nsec / NSEC_PER_SEC;
}

/*
 * Return a timeval struct derived from timespec <t>.
 */
struct timeval tsToTimeval(struct timespec t)
{
    struct timeval tv = {
        .tv_sec = t.tv_sec,
        .tv_usec = t.tv_nsec / 1000
    };

    return tv;
}

/*
 * Return a timespec struct derived from timeval <t>.
 */
struct timespec tsFromTimeval(struct timeval t)
{
    struct timespec ts = {
        .tv_sec = t.tv_sec,
        .tv_nsec = t.tv_usec * 1000
    };

    return ts;
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(void)
{
    struct timespec t0, t1;

    t0 = tsMake(1, 500000000L);
    check_timespec(t0, 1, 500000000L);

    t0 = tsMake(1, 1500000000L);
    check_timespec(t0, 2, 500000000L);

    t0 = tsMake(1, -500000000L);
    check_timespec(t0, 0, 500000000L);

    t0 = tsMake(-1, 1500000000L);
    check_timespec(t0, 0, 500000000L);

    t0 = tsMake(-1, -500000000L);
    check_timespec(t0, -2, 500000000L);

    t0 = tsMake(1, 0);
    t1 = tsMake(2, 0);

    make_sure_that(tsDiff(t1, t0) == 1);

    t0 = tsMake(1, 200000000L);
    t1 = tsMake(1, 700000000L);

    make_sure_that(tsDiff(t1, t0) == 0.5);

    t0 = tsMake(1, 700000000L);
    t1 = tsMake(1, 200000000L);

    make_sure_that(tsDiff(t1, t0) == -0.5);

    t1 = tsMake(2, 150000000L);
    t0 = tsMake(1, 900000000L);

    make_sure_that(tsDiff(t1, t0) == 0.25);

    t1 = tsMake(1, 900000000L);
    t0 = tsMake(2, 150000000L);

    make_sure_that(tsDiff(t1, t0) == -0.25);

    t0 = tsMake(1, 500000000L);
    check_timespec(tsAdd(t0, 1),    2, 500000000L);
    check_timespec(tsAdd(t0, 0.25), 1, 750000000L);
    check_timespec(tsSub(t0, 1),    0, 500000000L);
    check_timespec(tsSub(t0, 0.25), 1, 250000000L);

    t1 = tsMake(1, 0);
    t0 = tsMake(2, 0);

    make_sure_that(tsCompare(t1, t0) < 0);

    t1 = tsMake(2, 0);
    t0 = tsMake(1, 0);

    make_sure_that(tsCompare(t1, t0) > 0);

    t1 = tsMake(0, 0);
    t0 = tsMake(0, 1);

    make_sure_that(tsCompare(t1, t0) < 0);

    t1 = tsMake(0, 1);
    t0 = tsMake(0, 0);

    make_sure_that(tsCompare(t1, t0) > 0);

    t1 = tsMake(0, 0);
    t0 = tsMake(0, 0);

    make_sure_that(tsCompare(t1, t0) == 0);

    return errors;
}
#endif