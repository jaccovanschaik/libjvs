#include "timespec.c"
#include "utils.h"

static int errors = 0;

/*
 * Check that the timespec in <t> contains <sec> and <nsec>. If not, print a
 * message to that effect on stderr (using <file> and <line>, which should
 * contain the source file and line where this function was called) and
 * increment the error counter pointed to by <errors>. This function is used
 * in test code, and should be called using the check_timespec macro.
 */
void _check_timespec(const char *file, int line,
                     const char *name, int *errors,
                     struct timespec t, long sec, long nsec)
{
    if (t.tv_sec != sec || t.tv_nsec != nsec) {
        (*errors)++;
        fprintf(stderr, "%s:%d: %s = { %ld, %ld }, expected { %ld, %ld }\n",
                file, line, name, t.tv_sec, t.tv_nsec, sec, nsec);
    }
}

#define check_timespec(t, sec, nsec) \
    _check_timespec(__FILE__, __LINE__, #t, &errors, t, sec, nsec)

int main(void)
{
    char *s;
    struct timespec t0, t1;

    t0 = (struct timespec) { .tv_sec = 0, .tv_nsec = 1500000000L };
    t1 = tsNormalized(t0);
    check_timespec(t0, 0, 1500000000L);
    check_timespec(t1, 1,  500000000L);

    t0 = (struct timespec) { .tv_sec = 1, .tv_nsec = -500000000L };
    t1 = tsNormalized(t0);
    check_timespec(t0, 1, -500000000L);
    check_timespec(t1, 0,  500000000L);

    t0 = (struct timespec) { .tv_sec = 0, .tv_nsec = 1500000000L };
    tsNormalize(&t0);
    check_timespec(t0, 1,  500000000L);

    t0 = (struct timespec) { .tv_sec = 1, .tv_nsec = -500000000L };
    tsNormalize(&t0);
    check_timespec(t0, 0,  500000000L);

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

    make_sure_that(tsDelta(t1, t0) == 1);

    t0 = tsMake(1, 200000000L);
    t1 = tsMake(1, 700000000L);

    make_sure_that(tsDelta(t1, t0) == 0.5);

    t0 = tsMake(1, 700000000L);
    t1 = tsMake(1, 200000000L);

    make_sure_that(tsDelta(t1, t0) == -0.5);

    t1 = tsMake(2, 150000000L);
    t0 = tsMake(1, 900000000L);

    make_sure_that(tsDelta(t1, t0) == 0.25);

    t1 = tsMake(1, 900000000L);
    t0 = tsMake(2, 150000000L);

    make_sure_that(tsDelta(t1, t0) == -0.25);

    t0 = tsMake(5, 750000000L);
    check_timespec(tsInc(t0, 0.50), 6, 250000000L);
    check_timespec(tsInc(t0, 1.00), 6, 750000000L);
    check_timespec(tsInc(t0, 2.00), 7, 750000000L);
    check_timespec(tsDec(t0, 0.50), 5, 250000000L);
    check_timespec(tsDec(t0, 1.00), 4, 750000000L);
    check_timespec(tsDec(t0, 2.00), 3, 750000000L);

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

    t0 = tsMake(12 * 3600 + 34 * 60 + 56, 987654321);

    s = tsFormat(t0, "GMT", "%H:%M:%9S");
    make_sure_that(strcmp(s, "12:34:56.987654321") == 0);
    free(s);

    s = tsFormat(t0, "GMT", "%H:%M:%3S");
    make_sure_that(strcmp(s, "12:34:56.988") == 0);
    free(s);

    s = tsFormat(t0, "GMT", "%H:%M:%0S");
    make_sure_that(strcmp(s, "12:34:57") == 0);
    free(s);

    s = tsFormat(t0, "GMT", "%H:%M:%S");
    make_sure_that(strcmp(s, "12:34:56") == 0);
    free(s);

    s = tsFormat(t0, "UTC+1", "%H:%M:%S");
    make_sure_that(strcmp(s, "11:34:56") == 0);
    free(s);

    return errors;
}
