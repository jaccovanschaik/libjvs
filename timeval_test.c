#include "timeval.c"
#include "utils.h"

static int errors = 0;

/*
 * Check that the timeval in <t> contains <sec> and <nsec>. If not, print a
 * message to that effect on stderr (using <file> and <line>, which should
 * contain the source file and line where this function was called) and
 * increment the error counter pointed to by <errors>. This function should be
 * called using the check_timeval macro.
 */
static void _check_timeval(const char *file, int line,
                    const char *name, int *errors,
                    struct timeval t, long sec, long usec)
{
    if (t.tv_sec != sec || t.tv_usec != usec) {
        (*errors)++;
        fprintf(stderr, "%s:%d: %s = { %ld, %ld }, expected { %ld, %ld }\n",
                file, line, name, t.tv_sec, t.tv_usec, sec, usec);
    }
}

#define check_timeval(t, sec, usec) \
    _check_timeval(__FILE__, __LINE__, #t, &errors, t, sec, usec)

int main(void)
{
    char *s;
    struct timeval t0, t1;

    t0 = (struct timeval) { .tv_sec = 0, .tv_usec = 1500000L };
    t1 = tvNormalized(t0);
    check_timeval(t0, 0, 1500000L);
    check_timeval(t1, 1,  500000L);

    t0 = (struct timeval) { .tv_sec = 1, .tv_usec = -500000L };
    t1 = tvNormalized(t0);
    check_timeval(t0, 1, -500000L);
    check_timeval(t1, 0,  500000L);

    t0 = (struct timeval) { .tv_sec = 0, .tv_usec = 1500000L };
    tvNormalize(&t0);
    check_timeval(t0, 1,  500000L);

    t0 = (struct timeval) { .tv_sec = 1, .tv_usec = -500000L };
    tvNormalize(&t0);
    check_timeval(t0, 0,  500000L);

    t0 = tvMake(1, 500000L);
    check_timeval(t0, 1, 500000L);

    t0 = tvMake(1, 1500000L);
    check_timeval(t0, 2, 500000L);

    t0 = tvMake(1, -500000L);
    check_timeval(t0, 0, 500000L);

    t0 = tvMake(-1, 1500000L);
    check_timeval(t0, 0, 500000L);

    t0 = tvMake(-1, -500000L);
    check_timeval(t0, -2, 500000L);

    t0 = tvMake(1, 0);
    t1 = tvMake(2, 0);

    make_sure_that(tvDelta(t1, t0) == 1);

    t0 = tvMake(1, 200000L);
    t1 = tvMake(1, 700000L);

    make_sure_that(tvDelta(t1, t0) == 0.5);

    t0 = tvMake(1, 700000L);
    t1 = tvMake(1, 200000L);

    make_sure_that(tvDelta(t1, t0) == -0.5);

    t1 = tvMake(2, 150000L);
    t0 = tvMake(1, 900000L);

    make_sure_that(tvDelta(t1, t0) == 0.25);

    t1 = tvMake(1, 900000L);
    t0 = tvMake(2, 150000L);

    make_sure_that(tvDelta(t1, t0) == -0.25);

    t0 = tvMake(5, 750000L);
    check_timeval(tvInc(t0, 0.50), 6, 250000L);
    check_timeval(tvInc(t0, 1.00), 6, 750000L);
    check_timeval(tvInc(t0, 2.00), 7, 750000L);
    check_timeval(tvDec(t0, 0.50), 5, 250000L);
    check_timeval(tvDec(t0, 1.00), 4, 750000L);
    check_timeval(tvDec(t0, 2.00), 3, 750000L);

    t1 = tvMake(1, 0);
    t0 = tvMake(2, 0);

    make_sure_that(tvCompare(t1, t0) < 0);

    t1 = tvMake(2, 0);
    t0 = tvMake(1, 0);

    make_sure_that(tvCompare(t1, t0) > 0);

    t1 = tvMake(0, 0);
    t0 = tvMake(0, 1);

    make_sure_that(tvCompare(t1, t0) < 0);

    t1 = tvMake(0, 1);
    t0 = tvMake(0, 0);

    make_sure_that(tvCompare(t1, t0) > 0);

    t1 = tvMake(0, 0);
    t0 = tvMake(0, 0);

    make_sure_that(tvCompare(t1, t0) == 0);

    t0 = tvMake(12 * 3600 + 34 * 60 + 56, 987654);

    s = tvFormat(t0, "GMT", "%H:%M:%6S");
    make_sure_that(strcmp(s, "12:34:56.987654") == 0);
    free(s);

    s = tvFormat(t0, "GMT", "%H:%M:%3S");
    make_sure_that(strcmp(s, "12:34:56.988") == 0);
    free(s);

    s = tvFormat(t0, "GMT", "%H:%M:%0S");
    make_sure_that(strcmp(s, "12:34:57") == 0);
    free(s);

    s = tvFormat(t0, "GMT", "%H:%M:%S");
    make_sure_that(strcmp(s, "12:34:56") == 0);
    free(s);

    s = tvFormat(t0, "UTC+1", "%H:%M:%S");
    make_sure_that(strcmp(s, "11:34:56") == 0);
    free(s);

    return errors;
}
