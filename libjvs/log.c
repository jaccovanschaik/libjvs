/*
 * log.c: Handle logging.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
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
    OT_UDP,
    OT_TCP,
    OT_FILE,
    OT_FP,
    OT_FD,
    OT_SYSLOG
} LOG_OutputType;

/*
 * An output channel for logging.
 */
typedef struct {
    ListNode _node;
    LOG_OutputType type;
    union {
        FILE *fp;       /* OT_FILE, OT_FP */
        int fd;         /* OT_UDP, OT_TCP, OT_FD */
        int priority;   /* OT_SYSLOG */
    } u;
} LOG_Output;

/*
 * A logger.
 */
struct Logger {
    List outputs;
    Buffer scratch;
    int with_date, with_time, with_file, with_func, with_line;
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
    case OT_UDP:
    case OT_TCP:
        /* Opened as file descriptors. */
        close(out->u.fd);
        break;
    case OT_FILE:
        /* Opened as FILE pointers. */
        fclose(out->u.fp);
        break;
    case OT_FP:
    case OT_FD:
        /* Not opened by me, so leave them alone. */
        break;
    case OT_SYSLOG:
        /* Opened using openlog(). */
        closelog();
        break;
    }

    free(out);
}

/*
 * Create a new logger.
 */
Logger *logCreate(void)
{
    Logger *logger = calloc(1, sizeof(Logger));

    logger->with_time = -1;

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
        LOG_Output *out = log_create_output(logger, OT_UDP);
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
        LOG_Output *out = log_create_output(logger, OT_TCP);
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
        LOG_Output *out = log_create_output(logger, OT_FILE);
        out->u.fp = fp;
        return 0;
    }
}

/*
 * Add an output channel to the previously opened FILE pointer <fp>.
 */
int logToFP(Logger *logger, FILE *fp)
{
    LOG_Output *out = log_create_output(logger, OT_FP);

    out->u.fp = fp;

    return 0;
}

/*
 * Add an output channel to the previously opened file descriptor <fd>.
 */
int logToFD(Logger *logger, int fd)
{
    LOG_Output *out = log_create_output(logger, OT_FD);

    out->u.fd = fd;

    return 0;
}

/*
 * Add an output channel to the syslog facility using the given parameters (see openlog(3) for the
 * meaning of these parameters).
 */
int logToSyslog(Logger *logger, const char *ident, int option, int facility, int priority)
{
    LOG_Output *out = log_create_output(logger, OT_SYSLOG);

    openlog(ident, option, facility);

    out->u.priority = priority;

    return 0;
}

/*
 * Add (if <on> is TRUE) or leave out (if <on> is FALSE) a date of the form YYYY-MM-DD in output
 * messages. The default is FALSE.
 */
void logWithDate(Logger *logger, int on)
{
    logger->with_date = on;
}

/*
 * Add (if <on> is TRUE) or leave out (if <on> is FALSE) a timestamp of the form HH:MM:SS to output
 * messages. <precision> is the number of sub-second digits to show. The default is FALSE.
 */
void logWithTime(Logger *logger, int on, int precision)
{
    if (!on) {
        logger->with_time = -1;
    }
    else {
        logger->with_time = CLAMP(precision, 0, 6);
    }
}

/*
 * Add (if <on> is TRUE) or leave out (if <on> is FALSE) the name of the file from which the
 * logWrite function was called in log messages. The default is FALSE.
 */
void logWithFile(Logger *logger, int on)
{
    logger->with_file = on;
}

/*
 * Add (if <on> is TRUE) or leave out (if <on> is FALSE) the name of the function that called the
 * logWrite function. The default is FALSE.
 */
void logWithFunction(Logger *logger, int on)
{
    logger->with_func = on;
}

/*
 * Add (if <on> is TRUE) or leave out (if <on> is FALSE) the line number of the call to the logWrite
 * function. The default is FALSE.
 */
void logWithLine(Logger *logger, int on)
{
    logger->with_line = on;
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
    struct timeval tv;
    struct tm *tm;

    va_list ap;
    LOG_Output *out;

    if (logger->with_date || logger->with_time > -1) {
        gettimeofday(&tv, NULL);
        tm = localtime(&tv.tv_sec);
    }

    bufClear(&logger->scratch);

    if (logger->with_date) {
        bufAddF(&logger->scratch, "%04d-%02d-%02d ",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    }

    if (logger->with_time > -1) {
        bufAddF(&logger->scratch, "%02d:%02d:", tm->tm_hour, tm->tm_min);

        if (logger->with_time == 0) {
            bufAddF(&logger->scratch, "%02d ", tm->tm_sec);
        }
        else {
            double seconds = (double) tm->tm_sec + (double) tv.tv_usec / 1000000.0;

            bufAddF(&logger->scratch, "%0*.*f ",
                    3 + logger->with_time, logger->with_time, seconds);
        }
    }

    if (logger->with_file) {
        bufAddF(&logger->scratch, "%s ", _log_file);
    }

    if (logger->with_func) {
        bufAddF(&logger->scratch, "%s ", _log_func);
    }

    if (logger->with_line) {
        bufAddF(&logger->scratch, "%d ", _log_line);
    }

    va_start(ap, fmt);
    bufAddV(&logger->scratch, fmt, ap);
    va_end(ap);

    for (out = listHead(&logger->outputs); out; out = listNext(out)) {
        switch(out->type) {
        case OT_UDP:
        case OT_TCP:
        case OT_FD:
            tcpWrite(out->u.fd, bufGet(&logger->scratch), bufLen(&logger->scratch));
            break;
        case OT_FILE:
        case OT_FP:
            fwrite(bufGet(&logger->scratch), bufLen(&logger->scratch), 1, out->u.fp);
            fflush(out->u.fp);
            break;
        case OT_SYSLOG:
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
