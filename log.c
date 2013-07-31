/*
 * log.c: Write log messages.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "udp.h"
#include "tcp.h"
#include "list.h"
#include "defs.h"
#include "buffer.h"

#include "log.h"

/*
 * Out put types.
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
};

/*
 * Global variables to store the location of the latest log message.
 */
static const char *_log_file;
static int         _log_line;
static const char *_log_func;

/*
 * Create a logging output of <type> for <logger>..
 */
static LOG_Output *log_create_output(Logger *logger, LOG_OutputType type)
{
    LOG_Output *out = calloc(1, sizeof(LOG_Output));

    out->type = type;

    listAppendTail(&logger->outputs, out);

    return out;
}

/*
 * Close logging output <out>.
 */
static void log_close_output(LOG_Output *out)
{
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

static LOG_Prefix *log_add_prefix(Logger *logger, LOG_PrefixType type)
{
    LOG_Prefix *prefix = calloc(1, sizeof(LOG_Prefix));

    prefix->type = type;

    listAppendTail(&logger->prefixes, prefix);

    return prefix;
}

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

    return logger;
}

/*
 * Add an output channel that sends messages to a UDP socket on port <port> on host <host>. If the
 * host could not be found, -1 is returned and the channel is not created.
 */
int logToUDP(Logger *logger, const char *host, int port)
{
    int fd;

    if ((fd = udpConnect(host, port)) < 0)
        return -1;
    else {
        LOG_Output *out = log_create_output(logger, LOG_OT_UDP);
        out->u.fd = fd;
        return 0;
    }
}

/*
 * Add an output channel that sends messages over a TCP connection to port <port> on host <host>. If
 * no connection could be opened, -1 is returned and the channel is not created.
 */
int logToTCP(Logger *logger, const char *host, int port)
{
    int fd;

    if ((fd = tcpConnect(host, port)) < 0)
        return -1;
    else {
        LOG_Output *out = log_create_output(logger, LOG_OT_TCP);
        out->u.fd = fd;
        return 0;
    }
}

/*
 * Add an output channel to file <file>. If the file could not be opened, -1 is returned and the
 * channel is not created.
 */
int logToFile(Logger *logger, const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL)
        return -1;
    else {
        LOG_Output *out = log_create_output(logger, LOG_OT_FILE);
        out->u.fp = fp;
        return 0;
    }
}

/*
 * Add an output channel to the previously opened FILE pointer <fp>.
 */
int logToFP(Logger *logger, FILE *fp)
{
    LOG_Output *out = log_create_output(logger, LOG_OT_FP);

    out->u.fp = fp;

    return 0;
}

/*
 * Add an output channel to the previously opened file descriptor <fd>.
 */
int logToFD(Logger *logger, int fd)
{
    LOG_Output *out = log_create_output(logger, LOG_OT_FD);

    out->u.fd = fd;

    return 0;
}

/*
 * Add an output channel to the syslog facility using the given parameters (see openlog(3) for the
 * meaning of these parameters).
 */
int logToSyslog(Logger *logger, const char *ident, int option, int facility, int priority)
{
    LOG_Output *out = log_create_output(logger, LOG_OT_SYSLOG);

    openlog(ident, option, facility);

    out->u.priority = priority;

    return 0;
}

/*
 * Add a date of the form YYYY-MM-DD in output messages.
 */
void logWithDate(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_DATE);
}

/*
 * Add a timestamp of the form HH:MM:SS to output messages. <precision> is the number of sub-second
 * digits to show.
 */
void logWithTime(Logger *logger, int precision)
{
    LOG_Prefix *prefix = log_add_prefix(logger, LOG_PT_TIME);

    prefix->u.precision = CLAMP(precision, 0, 6);
}

/*
 * Add the name of the file from which the logWrite function was called to log messages.
 */
void logWithFile(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_FILE);
}

/*
 * Add the name of the function that called the logWrite function to log messages.
 */
void logWithFunction(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_FUNC);
}

/*
 * Add the line number of the call to the logWrite function to log messages
 */
void logWithLine(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_FUNC);
}

/*
 * Add string <string> to log messages.
 */
void logWithString(Logger *logger, const char *string)
{
    LOG_Prefix *prefix = log_add_prefix(logger, LOG_PT_STR);

    prefix->u.string = strdup(string);
}

/*
 * Set _log_file, _log_line and _log_func to the given location. Called as part of the logWrite
 * macro, not to be called on its own.
 */
void _logSetLocation(const char *file, int line, const char *func)
{
    _log_file = file;
    _log_line = line;
    _log_func = func;
}

/*
 * Send out a logging message using <fmt> and the subsequent parameters through <logger>. Called as
 * part of the logWrite macro, not to be called on its own.
 */
void _logWrite(Logger *logger, const char *fmt, ...)
{
    struct tm tm = { 0 };
    struct timeval tv = { 0 };

    va_list ap;
    LOG_Output *out;
    LOG_Prefix *pfx;

    bufClear(&logger->scratch);

    for (pfx = listHead(&logger->prefixes); pfx; pfx = listNext(pfx)) {
        switch(pfx->type) {
        case LOG_PT_DATE:
            if (tv.tv_sec == 0) log_get_time(&tm, &tv);

            bufAddF(&logger->scratch, "%04d-%02d-%02d ",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

            break;
        case LOG_PT_TIME:
            if (tv.tv_sec == 0) log_get_time(&tm, &tv);

            bufAddF(&logger->scratch, "%02d:%02d:", tm.tm_hour, tm.tm_min);

            if (pfx->u.precision == 0) {
                bufAddF(&logger->scratch, "%02d ", tm.tm_sec);
            }
            else {
                double seconds = (double) tm.tm_sec + (double) tv.tv_usec / 1000000.0;

                bufAddF(&logger->scratch, "%0*.*f ",
                        3 + pfx->u.precision, pfx->u.precision, seconds);
            }

            break;
        case LOG_PT_FILE:
            bufAddF(&logger->scratch, "%s ", _log_file);
            break;
        case LOG_PT_LINE:
            bufAddF(&logger->scratch, "%d ", _log_line);
            break;
        case LOG_PT_FUNC:
            bufAddF(&logger->scratch, "%s ", _log_func);
            break;
        case LOG_PT_STR:
            bufAddF(&logger->scratch, "%s ", pfx->u.string);
            break;
        }
    }

    va_start(ap, fmt);
    bufAddV(&logger->scratch, fmt, ap);
    va_end(ap);

    for (out = listHead(&logger->outputs); out; out = listNext(out)) {
        switch(out->type) {
        case LOG_OT_UDP:
        case LOG_OT_TCP:
        case LOG_OT_FD:
            tcpWrite(out->u.fd, bufGet(&logger->scratch), bufLen(&logger->scratch));
            break;
        case LOG_OT_FILE:
        case LOG_OT_FP:
            fwrite(bufGet(&logger->scratch), bufLen(&logger->scratch), 1, out->u.fp);
            fflush(out->u.fp);
            break;
        case LOG_OT_SYSLOG:
            syslog(out->u.priority, "%s", bufGet(&logger->scratch));
            break;
        }
    }
}

/*
 * Close logger <logger>.
 */
void logClose(Logger *logger)
{
    LOG_Output *out;

    while ((out = listRemoveHead(&logger->outputs)) != NULL) {
        log_close_output(out);
    }

    free(logger);
}
