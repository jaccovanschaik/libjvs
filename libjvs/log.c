/*
 * log.c: Description
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

typedef enum {
    OT_UDP,
    OT_TCP,
    OT_FILE,
    OT_FP,
    OT_FD,
    OT_SYSLOG
} LOG_OutputType;

typedef struct {
    ListNode _node;
    LOG_OutputType type;
    union {
        FILE *fp;
        int priority;
    } u;
} LOG_Output;

struct Logger {
    List outputs;
    Buffer scratch;
    int with_date, with_time, with_file, with_func, with_line;
};

static const char *_log_file;
static int         _log_line;
static const char *_log_func;

static LOG_Output *log_create_output(Logger *logger, LOG_OutputType type)
{
    LOG_Output *out = calloc(1, sizeof(LOG_Output));

    out->type = type;

    listAppendTail(&logger->outputs, out);

    return out;
}

static void log_close_output(LOG_Output *out)
{
    switch(out->type) {
    case OT_UDP:
    case OT_TCP:
    case OT_FILE:
    case OT_FD:
        fclose(out->u.fp);
        break;
    case OT_FP:
        break;
    case OT_SYSLOG:
        closelog();
        break;
    }

    free(out);
}

Logger *logCreate(void)
{
    Logger *logger = calloc(1, sizeof(Logger));

    logger->with_time = -1;

    return logger;
}

int logToUDP(Logger *logger, const char *host, int port)
{
    int fd;
    FILE *fp;

    if ((fd = udpConnect(host, port)) < 0)
        return 1;
    else if ((fp = fdopen(fd, "w")) == NULL)
        return 1;
    else {
        LOG_Output *out = log_create_output(logger, OT_UDP);
        out->u.fp = fp;
        return 0;
    }
}

int logToTCP(Logger *logger, const char *host, int port)
{
    int fd;
    FILE *fp;

    if ((fd = tcpConnect(host, port)) < 0)
        return 1;
    else if ((fp = fdopen(fd, "w")) == NULL)
        return 1;
    else {
        LOG_Output *out = log_create_output(logger, OT_TCP);
        out->u.fp = fp;
        return 0;
    }
}

int logToFile(Logger *logger, const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL)
        return 1;
    else {
        LOG_Output *out = log_create_output(logger, OT_FILE);
        out->u.fp = fp;
        return 0;
    }
}

int logToFP(Logger *logger, FILE *fp)
{
    LOG_Output *out = log_create_output(logger, OT_FP);
    out->u.fp = fp;

    return 0;
}

int logToFD(Logger *logger, int fd)
{
    FILE *fp;

    if ((fd = dup(fd)) == -1)
        return 1;
    else if ((fp = fdopen(fd, "w")) == NULL)
        return 1;
    else {
        LOG_Output *out = log_create_output(logger, OT_FD);
        out->u.fp = fp;
        return 0;
    }
}

int logToSyslog(Logger *logger, const char *ident, int option, int facility, int priority)
{
    LOG_Output *out = log_create_output(logger, OT_SYSLOG);

    openlog(ident, option, facility);

    out->u.priority = priority;

    return 0;
}

void logWithTime(Logger *logger, int on, int precision)
{
    if (!on) {
        logger->with_time = -1;
    }
    else {
        logger->with_time = CLAMP(precision, 0, 6);
    }
}

void logWithDate(Logger *logger, int on)
{
    logger->with_date = on;
}

void logWithFile(Logger *logger, int on)
{
    logger->with_file = on;
}

void logWithFunction(Logger *logger, int on)
{
    logger->with_func = on;
}

void logWithLine(Logger *logger, int on)
{
    logger->with_line = on;
}

void _logSetLocation(const char *file, int line, const char *func)
{
    _log_file = file;
    _log_line = line;
    _log_func = func;
}

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
        case OT_FILE:
        case OT_FP:
        case OT_FD:
            fwrite(bufGet(&logger->scratch), bufLen(&logger->scratch), 1, out->u.fp);
            fflush(out->u.fp);
            break;
        case OT_SYSLOG:
            syslog(out->u.priority, "%s", bufGet(&logger->scratch));
            break;
        }
    }
}

void logClose(Logger *logger)
{
    LOG_Output *out;

    while ((out = listRemoveHead(&logger->outputs)) != NULL) {
        log_close_output(out);
    }

    free(logger);
}
