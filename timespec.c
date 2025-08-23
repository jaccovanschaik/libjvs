/*
 * timespec.c: calculations with struct timespec's.
 *
 * Copyright: (c) 2020-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-10-22
 * Version:   $Id: timespec.c 507 2025-08-23 14:43:51Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "timespec.h"
#include "utils.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#define NSEC_PER_SEC 1000000000L

/*
 * Return a normalized version of <ts>, where tv_nsec lies in [0, 10^9> and
 * tv_sec is adjusted accordingly.
 */
struct timespec tsNormalized(struct timespec ts)
{
    struct timespec dup = ts;

    while (dup.tv_nsec < 0) {
        dup.tv_sec--;
        dup.tv_nsec += NSEC_PER_SEC;
    }

    while (dup.tv_nsec >= NSEC_PER_SEC) {
        dup.tv_sec++;
        dup.tv_nsec -= NSEC_PER_SEC;
    }

    return dup;
}

/*
 * Normalize <ts>: make sure ts->tv_nsec lies in [0, 10^9> and adjust
 * ts->tv_sec accordingly.
 */
void tsNormalize(struct timespec *ts)
{
    *ts = tsNormalized(*ts);
}

/*
 * Return a pointer to a new timespec, filled with the values in <sec> and
 * <nsec>, and normalized.
 */
struct timespec *tsCreate(time_t sec, long nsec)
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
struct timespec tsMake(time_t sec, long nsec)
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
double tsDelta(struct timespec t1, struct timespec t0)
{
    return t1.tv_sec - t0.tv_sec
         + (double) (t1.tv_nsec - t0.tv_nsec) / NSEC_PER_SEC;
}

/*
 * Subtract <seconds> from <ts> and return the result as a new timespec.
 */
struct timespec tsDec(struct timespec ts, double seconds)
{
    ts.tv_sec  -= (time_t) seconds;
    ts.tv_nsec -= (long) (NSEC_PER_SEC * fmod(seconds, 1));

    tsNormalize(&ts);

    return ts;
}

/*
 * Add <seconds> to <ts> and return the result as a new timespec.
 */
struct timespec tsInc(struct timespec ts, double seconds)
{
    ts.tv_sec  += (time_t) seconds;
    ts.tv_nsec += (long) (NSEC_PER_SEC * fmod(seconds, 1));

    tsNormalize(&ts);

    return ts;
}

/*
 * Return a timespec derived from the double time value in <t>.
 */
struct timespec tsFromDouble(double t)
{
    struct timespec ts =
        tsMake((typeof(ts.tv_sec)) t, fmod(NSEC_PER_SEC * t, NSEC_PER_SEC));

    return ts;
}

/*
 * Return a double precision time value derived from timespec.
 */
double tsToDouble(struct timespec ts)
{
    return (double) ts.tv_sec + (double) ts.tv_nsec / NSEC_PER_SEC;
}

/*
 * Return a timespec struct derived from timeval <tv>.
 */
struct timespec tsFromTimeval(struct timeval tv)
{
    struct timespec ts = {
        .tv_sec  = tv.tv_sec,
        .tv_nsec = tv.tv_usec * 1000
    };

    return ts;
}

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
                      const char *fmt)
{
    return t_format_c(ts.tv_sec, ts.tv_nsec, tz, fmt);
}

/*
 * Identical to tsFormatC() above, but returns a dynamically allocated string
 * that you should free() when you're done with it.
 */
char *tsFormat(const struct timespec ts, const char *tz, const char *fmt)
{
    return strdup(tsFormatC(ts, tz, fmt));
}
