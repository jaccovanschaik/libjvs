/*
 * timeval.c: calculations with struct timeval's.
 *
 * Copyright: (c) 2020-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-10-22
 * Version:   $Id: timeval.c 507 2025-08-23 14:43:51Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "timeval.h"
#include "utils.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#define USEC_PER_SEC 1000000L

/*
 * Return a normalized version of <tv>, where tv_usec lies in [0, 10^9> and
 * tv_sec is adjusted accordingly.
 */
struct timeval tvNormalized(struct timeval tv)
{
    struct timeval dup = tv;

    while (dup.tv_usec < 0) {
        dup.tv_sec--;
        dup.tv_usec += USEC_PER_SEC;
    }

    while (dup.tv_usec >= USEC_PER_SEC) {
        dup.tv_sec++;
        dup.tv_usec -= USEC_PER_SEC;
    }

    return dup;
}

/*
 * Normalize <tv>: make sure tv->tv_usec lies in [0, 10^9> and adjust
 * tv->tv_sec accordingly.
 */
void tvNormalize(struct timeval *tv)
{
    *tv = tvNormalized(*tv);
}

/*
 * Return a pointer to a new timeval, filled with the values in <sec> and
 * <usec>, and normalized.
 */
struct timeval *tvCreate(long sec, long usec)
{
    struct timeval *t = calloc(1, sizeof(*t));

    t->tv_sec  = sec;
    t->tv_usec = usec;

    tvNormalize(t);

    return t;
}

/*
 * Return a timeval set to the values in <sec> and <usec>, and normalized.
 */
struct timeval tvMake(long sec, long usec)
{
    struct timeval t = {
        .tv_sec = sec,
        .tv_usec = usec
    };

    tvNormalize(&t);

    return t;
}

/*
 * Return the current time as a struct timeval.
 */
struct timeval tvNow(void)
{
    struct timeval tv = { 0 };

    gettimeofday(&tv, NULL);

    return tv;
}

/*
 * Compare <t1> and <t0>. Returns -1 if <t1> is less than <t0>, 1 if <t1> is
 * greater than <t0> or 0 if they are equal.
 */
int tvCompare(struct timeval t1, struct timeval t0)
{
    if (t1.tv_sec < t0.tv_sec) {
        return -1;
    }
    else if (t1.tv_sec > t0.tv_sec) {
        return 1;
    }
    else if (t1.tv_usec < t0.tv_usec) {
        return -1;
    }
    else if (t1.tv_usec > t0.tv_usec) {
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Return the difference between t1 and t0 (i.e. t1 - t0) as a double.
 */
double tvDelta(struct timeval t1, struct timeval t0)
{
    return t1.tv_sec - t0.tv_sec
         + (double) (t1.tv_usec - t0.tv_usec) / USEC_PER_SEC;
}

/*
 * Subtract <seconds> from <tv> and return the result as a new timeval.
 */
struct timeval tvDec(struct timeval tv, double seconds)
{
    tv.tv_sec  -= (long) seconds;
    tv.tv_usec -= (long) (USEC_PER_SEC * fmod(seconds, 1));

    tvNormalize(&tv);

    return tv;
}

/*
 * Add <seconds> to <tv> and return the result as a new timeval.
 */
struct timeval tvInc(struct timeval tv, double seconds)
{
    tv.tv_sec  += (long) seconds;
    tv.tv_usec += (long) (USEC_PER_SEC * fmod(seconds, 1));

    tvNormalize(&tv);

    return tv;
}

/*
 * Return a timeval derived from the double time value in <t>.
 */
struct timeval tvFromDouble(double t)
{
    return tvMake((int) t, fmod(USEC_PER_SEC * t, USEC_PER_SEC));
}

/*
 * Return a double precision time value derived from timeval.
 */
double tvToDouble(struct timeval tv)
{
    return (double) tv.tv_sec + (double) tv.tv_usec / USEC_PER_SEC;
}

/*
 * Return a timeval struct derived from timespec <ts>.
 */
struct timeval tvFromTimespec(struct timespec ts)
{
    struct timeval tv = {
        .tv_sec  = ts.tv_sec,
        .tv_usec = ts.tv_nsec * 1000
    };

    return tv;
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
 * nearest second, based on the value of <usec>.
 *
 * Returns a statically allocated string that the caller isn't supposed to
 * modify. If you need a string to call your own, use strdup() or call
 * t_format() below.
 */
const char *tvFormatC(const struct timeval tv, const char *tz,
        const char *fmt)
{
    return t_format_c(tv.tv_sec, 1000 * tv.tv_usec, tz, fmt);
}

/*
 * Identical to tvFormatC() above, but returns a dynamically allocated string
 * that you should free() when you're done with it.
 */
char *tvFormat(const struct timeval tv, const char *tz, const char *fmt)
{
    return strdup(tvFormatC(tv, tz, fmt));
}
