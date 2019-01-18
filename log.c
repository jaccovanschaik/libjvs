/*
 * log.c: Write log messages.
 *
 * Part of libjvs.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>

#include "net.h"
#include "udp.h"
#include "tcp.h"
#include "list.h"
#include "defs.h"
#include "buffer.h"

#include "log.h"

/*
 * Output types.
 */
typedef enum {
    LOG_OT_UDP,
    LOG_OT_TCP,
    LOG_OT_FILE,
    LOG_OT_FP,
    LOG_OT_FD,
    LOG_OT_SYSLOG
} LOG_OutputType;

/*
 * An output channel for logging.
 */
typedef struct {
    ListNode _node;
    LOG_OutputType type;
    uint64_t channels;
    union {
        FILE *fp;       /* For LOG_OT_FILE, LOG_OT_FP */
        int fd;         /* For LOG_OT_UDP, LOG_OT_TCP, LOG_OT_FD */
        int priority;   /* For LOG_OT_SYSLOG */
    } u;
} LOG_Output;

/*
 * Prefix types.
 */
typedef enum {
    LOG_PT_DATE,
    LOG_PT_TIME,
    LOG_PT_FILE,
    LOG_PT_LINE,
    LOG_PT_FUNC,
    LOG_PT_STR
} LOG_PrefixType;

/*
 * Definition of a prefix.
 */
typedef struct {
    ListNode _node;
    LOG_PrefixType type;
    union {
        char *string;
        int precision;
    } u;
} LOG_Prefix;

static List outputs = { 0 };        /* List of outputs. */
static List prefixes = { 0 };       /* List of prefixes. */
static Buffer scratch = { 0 };      /* Scratch buffer for building messages. */

/* Mutexes to guard multi-threaded access to these. */

pthread_mutex_t outputs_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t prefixes_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scratch_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Add and return a logging output of type <type> on <channels>.
 */
static LOG_Output *log_create_output(LOG_OutputType type, uint64_t channels)
{
    LOG_Output *out = calloc(1, sizeof(LOG_Output));

    out->type = type;
    out->channels = channels;

    pthread_mutex_lock(&outputs_mutex);

    listAppendTail(&outputs, out);

    pthread_mutex_unlock(&outputs_mutex);

    return out;
}

/*
 * Add a prefix of type <type> to the output.
 */
static LOG_Prefix *log_add_prefix(LOG_PrefixType type)
{
    LOG_Prefix *prefix = calloc(1, sizeof(LOG_Prefix));

    prefix->type = type;

    pthread_mutex_lock(&prefixes_mutex);

    listAppendTail(&prefixes, prefix);

    pthread_mutex_unlock(&prefixes_mutex);

    return prefix;
}

/*
 * Fill <tm> and <tv> with the current time.
 */
static void log_get_time(struct tm *tm, struct timeval *tv)
{
    gettimeofday(tv, NULL);

    localtime_r(&tv->tv_sec, tm);
}

/*
 * Send logging on any of <channels> to a UDP socket on port <port> on host
 * <host>. If the host could not be found, -1 is returned and the channel is not
 * created.
 */
int logToUDP(uint64_t channels, const char *host, uint16_t port)
{
    int fd;

    if ((fd = udpSocket()) < 0 || netConnect(fd, host, port) != 0)
        return -1;
    else {
        LOG_Output *out = log_create_output(LOG_OT_UDP, channels);

        out->u.fd = fd;

        return 0;
    }
}

/*
 * Send logging on any of <channels> through a TCP connection to port <port> on
 * host <host>. If no connection could be opened, -1 is returned and the channel
 * is not created.
 */
int logToTCP(uint64_t channels, const char *host, uint16_t port)
{
    int fd;

    if ((fd = tcpConnect(host, port)) < 0)
        return -1;
    else {
        LOG_Output *out = log_create_output(LOG_OT_TCP, channels);

        out->u.fd = fd;

        return 0;
    }
}

/*
 * Write logging on any of <channels> to the file whose name is specified with
 * <fmt> and the subsequent parameters. If the file could not be opened, -1 is
 * returned and the channel is not created.
 */
int logToFile(uint64_t channels, const char *fmt, ...)
{
    va_list ap;

    FILE *fp;
    Buffer filename = { 0 };

    va_start(ap, fmt);
    bufSetV(&filename, fmt, ap);
    va_end(ap);

    if ((fp = fopen(bufGet(&filename), "w")) == NULL) {
        bufReset(&filename);

        return -1;
    }
    else {
        LOG_Output *out = log_create_output(LOG_OT_FILE, channels);

        out->u.fp = fp;

        bufReset(&filename);

        return 0;
    }
}

/*
 * Write logging on any of <channels> to the previously opened FILE pointer
 * <fp>.
 */
int logToFP(uint64_t channels, FILE *fp)
{
    LOG_Output *out = log_create_output(LOG_OT_FP, channels);

    out->u.fp = fp;

    return 0;
}

/*
 * Write logging on any of <channels> to the previously opened file descriptor
 * <fd>.
 */
int logToFD(uint64_t channels, int fd)
{
    LOG_Output *out = log_create_output(LOG_OT_FD, channels);

    out->u.fd = fd;

    return 0;
}

/*
 * Write logging on any of <channels> to the syslog facility using the given
 * parameters (see openlog(3) for the meaning of these parameters).
 */
int logToSyslog(uint64_t channels, const char *ident, int option, int facility, int priority)
{
    LOG_Output *out = log_create_output(LOG_OT_SYSLOG, channels);

    openlog(ident, option, facility);

    out->u.priority = priority;

    return 0;
}

/*
 * Add a date of the form YYYY-MM-DD in output messages.
 */
void logWithDate(void)
{
    log_add_prefix(LOG_PT_DATE);
}

/*
 * Add a timestamp of the form HH:MM:SS to output messages. <precision> is the
 * number of sub-second digits to show.
 */
void logWithTime(int precision)
{
    LOG_Prefix *prefix = log_add_prefix(LOG_PT_TIME);

    prefix->u.precision = CLAMP(precision, 0, 6);
}

/*
 * Add the name of the file from which the logWrite function was called to log
 * messages.
 */
void logWithFile(void)
{
    log_add_prefix(LOG_PT_FILE);
}

/*
 * Add the name of the function that called the logWrite function to log
 * messages.
 */
void logWithFunction(void)
{
    log_add_prefix(LOG_PT_FUNC);
}

/*
 * Add the line number of the call to the logWrite function to log messages
 */
void logWithLine(void)
{
    log_add_prefix(LOG_PT_LINE);
}

/*
 * Add a string defined by <fmt> and the subsequent arguments to log messages.
 */
void logWithString(const char *fmt, ...)
{
    va_list ap;
    Buffer string = { 0 };

    LOG_Prefix *prefix = log_add_prefix(LOG_PT_STR);

    va_start(ap, fmt);
    bufSetV(&string, fmt, ap);
    va_end(ap);

    prefix->u.string = bufDetach(&string);
}

/*
 * Write the requested prefixes into the log message.
 */
static void log_write_prefixes(const char *file, int line, const char *func)
{
    struct tm tm = { 0 };
    struct timeval tv = { 0 };

    LOG_Prefix *pfx;

    tv.tv_sec = -1;

    for (pfx = listHead(&prefixes); pfx; pfx = listNext(pfx)) {
        switch(pfx->type) {
        case LOG_PT_DATE:
            if (tv.tv_sec == -1) log_get_time(&tm, &tv);

            bufAddF(&scratch, "%04d-%02d-%02d ",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

            break;
        case LOG_PT_TIME:
            if (tv.tv_sec == -1) log_get_time(&tm, &tv);

            bufAddF(&scratch, "%02d:%02d:", tm.tm_hour, tm.tm_min);

            if (pfx->u.precision == 0) {
                bufAddF(&scratch, "%02d ", tm.tm_sec);
            }
            else {
                double seconds =
                    (double) tm.tm_sec + (double) tv.tv_usec / 1000000.0;

                bufAddF(&scratch, "%0*.*f ",
                        3 + pfx->u.precision, pfx->u.precision, seconds);
            }

            break;
        case LOG_PT_FILE:
            bufAddF(&scratch, "%s ", file);
            break;
        case LOG_PT_LINE:
            bufAddF(&scratch, "%d ", line);
            break;
        case LOG_PT_FUNC:
            bufAddF(&scratch, "%s ", func);
            break;
        case LOG_PT_STR:
            bufAddF(&scratch, "%s ", pfx->u.string);
            break;
        }
    }
}

/*
 * Write a log message out to <channels>.
 */
static void log_output(uint64_t channels)
{
    LOG_Output *out;

    for (out = listHead(&outputs); out; out = listNext(out)) {
        if ((channels & out->channels) == 0) continue;

        switch(out->type) {
        case LOG_OT_UDP:
        case LOG_OT_TCP:
        case LOG_OT_FD:
            tcpWrite(out->u.fd, bufGet(&scratch),
                    bufLen(&scratch));
            break;
        case LOG_OT_FILE:
        case LOG_OT_FP:
            fwrite(bufGet(&scratch),
                    bufLen(&scratch), 1, out->u.fp);
            fflush(out->u.fp);
            break;
        case LOG_OT_SYSLOG:
            syslog(out->u.priority, "%s", bufGet(&scratch));
            break;
        }
    }
}

/*
 * Send out a logging message using <fmt> and the subsequent parameters to
 * <channels>. <file>, <line> and <func> are filled in by the logWrite macro,
 * which should be used to call this function.
 */
void _logWrite(uint64_t channels,
        const char *file, int line, const char *func, const char *fmt, ...)
{
    va_list ap;

    pthread_mutex_lock(&scratch_mutex);

    bufClear(&scratch);

    log_write_prefixes(file, line, func);

    va_start(ap, fmt);
    bufAddV(&scratch, fmt, ap);
    va_end(ap);

    log_output(channels);

    pthread_mutex_unlock(&scratch_mutex);
}

/*
 * Send a logging message using <fmt> and the subsequent parameters to
 * <channels>, *without* any prefixes. Useful to continue a previous log
 * message.
 */
void logAppend(uint64_t channels, const char *fmt, ...)
{
    va_list ap;

    pthread_mutex_lock(&scratch_mutex);

    bufClear(&scratch);

    va_start(ap, fmt);
    bufAddV(&scratch, fmt, ap);
    va_end(ap);

    log_output(channels);

    pthread_mutex_unlock(&scratch_mutex);
}
