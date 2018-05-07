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
    union {
        FILE *fp;       /* LOG_OT_FILE, LOG_OT_FP */
        int fd;         /* LOG_OT_UDP, LOG_OT_TCP, LOG_OT_FD */
        int priority;   /* LOG_OT_SYSLOG */
    } u;
    pthread_mutex_t output;
} LOG_Output;

typedef enum {
    LOG_PT_DATE,
    LOG_PT_TIME,
    LOG_PT_FILE,
    LOG_PT_LINE,
    LOG_PT_FUNC,
    LOG_PT_STR
} LOG_PrefixType;

typedef struct {
    ListNode _node;
    LOG_PrefixType type;
    union {
        char *string;
        int precision;
    } u;
} LOG_Prefix;

/*
 * A logger.
 */
struct Logger {
    List outputs;
    List prefixes;
    Buffer scratch;
    pthread_mutex_t access;
};

/*
 * Add and return a logging output of type <type> to <logger>.
 */
static LOG_Output *log_create_output(Logger *logger, LOG_OutputType type)
{
    LOG_Output *out = calloc(1, sizeof(LOG_Output));

    out->type = type;

    pthread_mutex_init(&out->output, NULL);

    listAppendTail(&logger->outputs, out);

    return out;
}

/*
 * Close logging output <out>.
 */
static void log_close_output(LOG_Output *out)
{
    pthread_mutex_destroy(&out->output);

    switch(out->type) {
    case LOG_OT_UDP:
    case LOG_OT_TCP:
        /* Opened as file descriptors. */
        close(out->u.fd);
        break;
    case LOG_OT_FILE:
        /* Opened as FILE pointers. */
        fclose(out->u.fp);
        break;
    case LOG_OT_FP:
    case LOG_OT_FD:
        /* Not opened by me, so leave them alone. */
        break;
    case LOG_OT_SYSLOG:
        /* Opened using openlog(). */
        closelog();
        break;
    }

    free(out);
}

/*
 * Add a prefix of type <type> to the output of <logger>.
 */
static LOG_Prefix *log_add_prefix(Logger *logger, LOG_PrefixType type)
{
    LOG_Prefix *prefix = calloc(1, sizeof(LOG_Prefix));

    prefix->type = type;

    listAppendTail(&logger->prefixes, prefix);

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
 * Create a new logger.
 */
Logger *logCreate(void)
{
    Logger *logger = calloc(1, sizeof(Logger));

    pthread_mutex_init(&logger->access, NULL);

    return logger;
}

/*
 * Add an output channel to <logger> that sends messages to a UDP socket on port
 * <port> on host <host>. If the host could not be found, -1 is returned and the
 * channel is not created.
 */
int logToUDP(Logger *logger, const char *host, uint16_t port)
{
    int fd;

    if ((fd = udpSocket()) < 0 || netConnect(fd, host, port) != 0)
        return -1;
    else {
        pthread_mutex_lock(&logger->access);

        LOG_Output *out = log_create_output(logger, LOG_OT_UDP);
        out->u.fd = fd;

        pthread_mutex_unlock(&logger->access);

        return 0;
    }
}

/*
 * Add an output channel to <logger> that sends messages over a TCP connection
 * to port <port> on host <host>. If no connection could be opened, -1 is
 * returned and the channel is not created.
 */
int logToTCP(Logger *logger, const char *host, uint16_t port)
{
    int fd;

    if ((fd = tcpConnect(host, port)) < 0)
        return -1;
    else {
        pthread_mutex_lock(&logger->access);

        LOG_Output *out = log_create_output(logger, LOG_OT_TCP);
        out->u.fd = fd;

        pthread_mutex_unlock(&logger->access);

        return 0;
    }
}

/*
 * Add an output channel to <logger> that writes to the file specified with
 * <fmt> and the subsequent parameters. If the file could not be opened, -1 is
 * returned and the channel is not created.
 */
int logToFile(Logger *logger, const char *fmt, ...)
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
        pthread_mutex_lock(&logger->access);

        LOG_Output *out = log_create_output(logger, LOG_OT_FILE);
        out->u.fp = fp;

        pthread_mutex_unlock(&logger->access);

        bufReset(&filename);

        return 0;
    }
}

/*
 * Add an output channel to <logger> that writes to the previously opened FILE
 * pointer <fp>.
 */
int logToFP(Logger *logger, FILE *fp)
{
    pthread_mutex_lock(&logger->access);

    LOG_Output *out = log_create_output(logger, LOG_OT_FP);

    out->u.fp = fp;

    pthread_mutex_unlock(&logger->access);

    return 0;
}

/*
 * Add an output channel to <logger> that writes to the previously opened file
 * descriptor <fd>.
 */
int logToFD(Logger *logger, int fd)
{
    pthread_mutex_lock(&logger->access);

    LOG_Output *out = log_create_output(logger, LOG_OT_FD);

    out->u.fd = fd;

    pthread_mutex_unlock(&logger->access);

    return 0;
}

/*
 * Add an output channel to <logger> that writes to the syslog facility using
 * the given parameters (see openlog(3) for the meaning of these parameters).
 */
int logToSyslog(Logger *logger,
        const char *ident, int option, int facility, int priority)
{
    pthread_mutex_lock(&logger->access);

    LOG_Output *out = log_create_output(logger, LOG_OT_SYSLOG);

    openlog(ident, option, facility);

    out->u.priority = priority;

    pthread_mutex_unlock(&logger->access);

    return 0;
}

/*
 * Add a date of the form YYYY-MM-DD in output messages.
 */
void logWithDate(Logger *logger)
{
    pthread_mutex_lock(&logger->access);

    log_add_prefix(logger, LOG_PT_DATE);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Add a timestamp of the form HH:MM:SS to output messages. <precision> is the
 * number of sub-second digits to show.
 */
void logWithTime(Logger *logger, int precision)
{
    pthread_mutex_lock(&logger->access);

    LOG_Prefix *prefix = log_add_prefix(logger, LOG_PT_TIME);

    prefix->u.precision = CLAMP(precision, 0, 6);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Add the name of the file from which the logWrite function was called to log
 * messages.
 */
void logWithFile(Logger *logger)
{
    pthread_mutex_lock(&logger->access);

    log_add_prefix(logger, LOG_PT_FILE);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Add the name of the function that called the logWrite function to log
 * messages.
 */
void logWithFunction(Logger *logger)
{
    pthread_mutex_lock(&logger->access);

    log_add_prefix(logger, LOG_PT_FUNC);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Add the line number of the call to the logWrite function to log messages
 */
void logWithLine(Logger *logger)
{
    pthread_mutex_lock(&logger->access);

    log_add_prefix(logger, LOG_PT_LINE);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Add a string defined by <fmt> and the subsequent arguments to log messages.
 */
void logWithString(Logger *logger, const char *fmt, ...)
{
    va_list ap;
    Buffer string = { 0 };

    pthread_mutex_lock(&logger->access);

    LOG_Prefix *prefix = log_add_prefix(logger, LOG_PT_STR);

    va_start(ap, fmt);
    bufSetV(&string, fmt, ap);
    va_end(ap);

    prefix->u.string = bufDetach(&string);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Write the requested prefixes into the log message.
 */
static void log_write_prefixes(Logger *logger,
        const char *file, int line, const char *func)
{
    struct tm tm = { 0 };
    struct timeval tv = { 0 };

    LOG_Prefix *pfx;

    tv.tv_sec = -1;

    for (pfx = listHead(&logger->prefixes); pfx; pfx = listNext(pfx)) {
        switch(pfx->type) {
        case LOG_PT_DATE:
            if (tv.tv_sec == -1) log_get_time(&tm, &tv);

            bufAddF(&logger->scratch, "%04d-%02d-%02d ",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

            break;
        case LOG_PT_TIME:
            if (tv.tv_sec == -1) log_get_time(&tm, &tv);

            bufAddF(&logger->scratch, "%02d:%02d:", tm.tm_hour, tm.tm_min);

            if (pfx->u.precision == 0) {
                bufAddF(&logger->scratch, "%02d ", tm.tm_sec);
            }
            else {
                double seconds =
                    (double) tm.tm_sec + (double) tv.tv_usec / 1000000.0;

                bufAddF(&logger->scratch, "%0*.*f ",
                        3 + pfx->u.precision, pfx->u.precision, seconds);
            }

            break;
        case LOG_PT_FILE:
            bufAddF(&logger->scratch, "%s ", file);
            break;
        case LOG_PT_LINE:
            bufAddF(&logger->scratch, "%d ", line);
            break;
        case LOG_PT_FUNC:
            bufAddF(&logger->scratch, "%s ", func);
            break;
        case LOG_PT_STR:
            bufAddF(&logger->scratch, "%s ", pfx->u.string);
            break;
        }
    }
}

/*
 * Write a log message out to all output channels.
 */
static void log_output(Logger *logger)
{
    LOG_Output *out;

    for (out = listHead(&logger->outputs); out; out = listNext(out)) {
        pthread_mutex_lock(&out->output);

        switch(out->type) {
        case LOG_OT_UDP:
        case LOG_OT_TCP:
        case LOG_OT_FD:
            tcpWrite(out->u.fd, bufGet(&logger->scratch),
                    bufLen(&logger->scratch));
            break;
        case LOG_OT_FILE:
        case LOG_OT_FP:
            fwrite(bufGet(&logger->scratch),
                    bufLen(&logger->scratch), 1, out->u.fp);
            fflush(out->u.fp);
            break;
        case LOG_OT_SYSLOG:
            syslog(out->u.priority, "%s", bufGet(&logger->scratch));
            break;
        }

        pthread_mutex_unlock(&out->output);
    }
}

/*
 * Send out a logging message using <fmt> and the subsequent parameters through
 * <logger>. <file>, <line> and <func> are filled in by the logWrite macro,
 * which should be used to call this function.
 */
void _logWrite(Logger *logger,
        const char *file, int line, const char *func, const char *fmt, ...)
{
    va_list ap;

    pthread_mutex_lock(&logger->access);

    bufClear(&logger->scratch);

    log_write_prefixes(logger, file, line, func);

    va_start(ap, fmt);
    bufAddV(&logger->scratch, fmt, ap);
    va_end(ap);

    log_output(logger);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Log <fmt> and the subsequent parameters through <logger>, *without* any
 * prefixes. Useful to continue a previous log message.
 */
void logAppend(Logger *logger, const char *fmt, ...)
{
    va_list ap;

    pthread_mutex_lock(&logger->access);

    bufClear(&logger->scratch);

    va_start(ap, fmt);
    bufAddV(&logger->scratch, fmt, ap);
    va_end(ap);

    log_output(logger);

    pthread_mutex_unlock(&logger->access);
}

/*
 * Close logger <logger>.
 */
void logClose(Logger *logger)
{
    LOG_Output *out;
    LOG_Prefix *pfx;

    if (logger == NULL) {
        return;
    }

    pthread_mutex_lock(&logger->access);

    bufReset(&logger->scratch);

    while ((out = listRemoveHead(&logger->outputs)) != NULL) {
        log_close_output(out);
    }

    while ((pfx = listRemoveHead(&logger->prefixes)) != NULL) {
        if (pfx->type == LOG_PT_STR && pfx->u.string != NULL) {
            free(pfx->u.string);
        }

        free(pfx);
    }

    pthread_mutex_unlock(&logger->access);

    pthread_mutex_destroy(&logger->access);

    free(logger);
}
