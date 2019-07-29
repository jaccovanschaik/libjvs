/*
 * log.c: XXX
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-29
 * Version:   $Id: log.c 334 2019-07-29 18:07:55Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#define _GNU_SOURCE

#include "log.h"

#include "net.h"
#include "tcp.h"
#include "udp.h"
#include "list.h"
#include "buffer.h"
#include "defs.h"
#include "utils.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>

typedef enum {
    LOG_PT_UTIME,
    LOG_PT_LTIME,
    LOG_PT_FILE,
    LOG_PT_LINE,
    LOG_PT_FUNC,
    LOG_PT_CHAN,
    LOG_PT_STR
} LogPrefixType;

typedef struct {
    ListNode _node;
    LogPrefixType type;
    char *str;
} LogPrefix;

typedef enum {
    LOG_OT_FILE,
    LOG_OT_FP,
    LOG_OT_FD,
    LOG_OT_TCP,
    LOG_OT_UDP,
    LOG_OT_SYSLOG,
    LOG_OT_FUNC
} LogWriterType;

struct LogChannel {
    ListNode _node;
    char *name;
    List writer_refs;
};

struct LogWriter {
    ListNode _node;
    LogWriterType type;
    List prefixes;
    char *host;
    uint16_t port;
    char *file;
    FILE *fp;
    int fd;
    int priority;
    void (*handler)(const char *msg, void *udata);
    char *separator;
    void *udata;
};

typedef struct {
    ListNode _node;
    LogWriter *writer;
} LogWriterRef;

static List channels = { 0 };
static List writers = { 0 };

static void log_get_time(struct timeval *tv)
{
#ifdef TEST
    tv->tv_sec  = 12 * 3600 + 34 * 60 + 56;
    tv->tv_usec = 123456;
#else
    gettimeofday(tv, NULL);
#endif
}

static void log_add_time(Buffer *buf, struct tm *tm, struct timeval *tv, const char *fmt)
{
    static regex_t *regex = NULL;

    char buffer[80];
    char new_fmt[80];
    const size_t nmatch = 2;
    regmatch_t pmatch[nmatch];

    if (regex == NULL) {
        regex = calloc(1, sizeof(*regex));
        regcomp(regex, "%([0-9]*)N", REG_EXTENDED);
    }

    if (regexec(regex, fmt, nmatch, pmatch, 0) == 0) {
        int precision;

        sscanf(fmt + pmatch[1].rm_so, "%d", &precision);

        precision = CLAMP(precision, 0, 6);

        double remainder = (double) tv->tv_usec / 1000000.0;

        char rem_buffer[10];

        snprintf(rem_buffer, sizeof(rem_buffer), "%0*.*f", 2 + precision, precision, remainder);

        snprintf(new_fmt,
                sizeof(new_fmt), "%.*s%s%s",
                pmatch[0].rm_so, fmt, rem_buffer + 2, fmt + pmatch[0].rm_eo);

        fmt = new_fmt;
    }

    strftime(buffer, sizeof(buffer), fmt, tm);

    bufAddS(buf, buffer);
}

static void log_add_prefixes(List *prefixes, Buffer *buf,
        const char *separator, const char *chan, const char *file, int line, const char *func)
{
    struct tm tm_local = { 0 }, tm_utc = { 0 };
    static struct timeval tv = { 0 };

    LogPrefix *pfx, *next_pfx;

    tv.tv_sec = -1;
    tm_utc.tm_sec = -1;
    tm_local.tm_sec = -1;

    for (pfx = listHead(prefixes); pfx; pfx = next_pfx) {
        next_pfx = listNext(pfx);

        switch(pfx->type) {
        case LOG_PT_LTIME:
            if (tv.tv_sec == -1) log_get_time(&tv);
            if (tm_local.tm_sec == -1) localtime_r(&tv.tv_sec, &tm_local);

            log_add_time(buf, &tm_local, &tv, pfx->str);

            break;
        case LOG_PT_UTIME:
            if (tv.tv_sec == -1) log_get_time(&tv);
            if (tm_utc.tm_sec == -1) gmtime_r(&tv.tv_sec, &tm_utc);

            log_add_time(buf, &tm_utc, &tv, pfx->str);

            break;
        case LOG_PT_FILE:
            bufAddF(buf, "%s", file);
            break;
        case LOG_PT_LINE:
            bufAddF(buf, "%d", line);
            break;
        case LOG_PT_FUNC:
            bufAddF(buf, "%s", func);
            break;
        case LOG_PT_CHAN:
            bufAddF(buf, "%s", chan);
            break;
        case LOG_PT_STR:
            bufAddF(buf, "%s", pfx->str);
            break;
        }

        if (pfx->type != LOG_PT_STR &&
                (next_pfx == NULL || next_pfx->type != LOG_PT_STR)) {
            bufAddS(buf, separator ? separator : " ");
        }
    }
}

static void log_write(LogWriter *writer, const char *buf)
{
    size_t len = strlen(buf);

    switch(writer->type) {
    case LOG_OT_UDP:
    case LOG_OT_TCP:
    case LOG_OT_FD:
        tcpWrite(writer->fd, buf, len);
        break;
    case LOG_OT_FILE:
    case LOG_OT_FP:
        fwrite(buf, len, 1, writer->fp);
        break;
    case LOG_OT_SYSLOG:
        syslog(writer->priority, "%s", buf);
        break;
    case LOG_OT_FUNC:
        writer->handler(buf, writer->udata);
        break;
    }
}

static LogPrefix *log_add_prefix(LogWriter *writer, LogPrefixType type)
{
    LogPrefix *prefix = calloc(1, sizeof(LogPrefix));

    prefix->type = type;

    listAppendTail(&writer->prefixes, prefix);

    return prefix;
}

static LogWriter *log_add_writer(LogWriterType type)
{
    LogWriter *writer = calloc(1, sizeof(*writer));

    writer->type = type;

    return writer;
}

static LogWriter *log_find_file_writer(const char *filename)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FILE && strcmp(writer->file, filename) == 0) break;
    }

    return writer;
}

static LogWriter *log_add_file_writer(const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL) {
        return NULL;
    }

    LogWriter *writer = log_add_writer(LOG_OT_FILE);

    writer->file = strdup(filename);
    writer->fp = fp;

    return writer;
}

static LogWriter *log_find_fp_writer(FILE *fp)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FP && writer->fp == fp) break;
    }

    return writer;
}

static LogWriter *log_add_fp_writer(FILE *fp)
{
    LogWriter *writer = log_add_writer(LOG_OT_FP);

    writer->fp = fp;

    return writer;
}

static LogWriter *log_find_fd_writer(int fd)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FD && writer->fd == fd) break;
    }

    return writer;
}

static LogWriter *log_add_fd_writer(int fd)
{
    LogWriter *writer = log_add_writer(LOG_OT_FD);

    writer->fd = fd;

    return writer;
}

static LogWriter *log_add_syslog_writer(const char *ident, int option,
        int facility, int priority)
{
    LogWriter *writer = log_add_writer(LOG_OT_SYSLOG);

    openlog(ident, option, facility);

    writer->priority = priority;

    return writer;
}

static LogWriter *log_find_udp_writer(const char *host, uint16_t port)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_UDP &&
            strcmp(writer->host, host) == 0 &&
            writer->port == port) break;
    }

    return writer;
}

static LogWriter *log_add_udp_writer(const char *host, uint16_t port)
{
    int fd;

    if ((fd = udpSocket()) < 0 || netConnect(fd, host, port) != 0) {
        return NULL;
    }

    LogWriter *writer = log_add_writer(LOG_OT_UDP);

    writer->host = strdup(host);
    writer->port = port;
    writer->fd   = fd;

    return writer;
}

static LogWriter *log_find_tcp_writer(const char *host, uint16_t port)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_TCP &&
            strcmp(writer->host, host) == 0 &&
            writer->port == port) break;
    }

    return writer;
}

static LogWriter *log_add_tcp_writer(const char *host, uint16_t port)
{
    int fd;

    if ((fd = tcpConnect(host, port)) < 0) {
        return NULL;
    }

    LogWriter *writer = log_add_writer(LOG_OT_TCP);

    writer->host = strdup(host);
    writer->port = port;
    writer->fd   = fd;

    return writer;
}

static LogWriter *log_find_function_writer(
        void (*handler)(const char *msg, void *udata),
        void *udata)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FUNC &&
            writer->handler == handler &&
            writer->udata == udata) break;
    }

    return writer;
}

static LogWriter *log_add_function_writer(
        void (*handler)(const char *msg, void *udata),
        void *udata)
{
    LogWriter *writer = log_add_writer(LOG_OT_FUNC);

    writer->handler = handler;
    writer->udata = udata;

    return writer;
}

LogWriter *logFileWriter(const char *fmt, ...)
{
    char *filename;
    va_list ap;

    va_start(ap, fmt);
    int r = asprintf(&filename, fmt, ap);
    va_end(ap);

    if (r == -1) return NULL;

    LogWriter *writer;

    if ((writer = log_find_file_writer(filename)) == NULL &&
        (writer = log_add_file_writer(filename)) == NULL) {
        return NULL;
    }
    else {
        return writer;
    }
}

LogWriter *logFPWriter(FILE *fp)
{
    LogWriter *writer;

    if ((writer = log_find_fp_writer(fp)) == NULL && 
        (writer = log_add_fp_writer(fp)) == NULL) {
        return NULL;
    }
    else {
        return writer;
    }
}

LogWriter *logFDWriter(int fd)
{
    LogWriter *writer;

    if ((writer = log_find_fd_writer(fd)) == NULL &&
        (writer = log_add_fd_writer(fd)) == NULL) {
        return NULL;
    }
    else {
        return writer;
    }
}

LogWriter *logTCPWriter(const char *host, uint16_t port)
{
    LogWriter *writer;

    if ((writer = log_find_tcp_writer(host, port)) == NULL &&
        (writer = log_add_tcp_writer(host, port)) == NULL) {
        return NULL;
    }
    else {
        return writer;
    }
}

LogWriter *logUDPWriter(const char *host, uint16_t port)
{
    LogWriter *writer;

    if ((writer = log_find_udp_writer(host, port)) == NULL &&
        (writer = log_add_udp_writer(host, port)) == NULL) {
        return NULL;
    }
    else {
        return writer;
    }
}

LogWriter *logSyslogWriter(const char *ident, int option, int facility, int priority)
{
    return log_add_syslog_writer(ident, option, facility, priority);
}

LogWriter *logFunctionWriter(void (*handler)(const char *msg, void *udata), void *udata)
{
    LogWriter *writer;

    if ((writer = log_find_function_writer(handler, udata)) == NULL &&
        (writer = log_add_function_writer(handler, udata)) == NULL) {
        return NULL;
    }
    else {
        return writer;
    }
}

void logWithLocalTime(LogWriter *writer, const char *fmt)
{
    LogPrefix *prefix = log_add_prefix(writer, LOG_PT_LTIME);

    prefix->str = strdup(fmt);
}

void logWithUniversalTime(LogWriter *writer, const char *fmt)
{
    LogPrefix *prefix = log_add_prefix(writer, LOG_PT_UTIME);

    prefix->str = strdup(fmt);
}

void logWithFile(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_FILE);
}

void logWithLine(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_LINE);
}

void logWithFunction(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_FUNC);
}

void logWithChannel(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_CHAN);
}

void logWithString(LogWriter *writer, const char *fmt, ...)
{
    char *str;
    va_list ap;

    va_start(ap, fmt);
    int r = asprintf(&str, fmt, ap);
    va_end(ap);

    if (r != -1) {
        LogPrefix *prefix = log_add_prefix(writer, LOG_PT_STR);

        prefix->str = str;
    }
}

void logWithSeparator(LogWriter *writer, const char *sep)
{
    free(writer->separator);

    writer->separator = strdup(sep);
}

void logCloseWriter(LogWriter *writer) 
{
    for (LogChannel *chan = listHead(&channels); chan; chan = listNext(chan)) {
        LogWriterRef *next_ref;

        for (LogWriterRef *ref = listHead(&chan->writer_refs); ref; ref = next_ref) {
            next_ref = listNext(ref);

            if (ref->writer == writer) {
                listRemove(&chan->writer_refs, ref);
                free(ref);
            }
        }
    }

    switch(writer->type) {
    case LOG_OT_UDP:
    case LOG_OT_TCP:
        /* Opened as file descriptors. */
        close(writer->fd);
        break;
    case LOG_OT_FILE:
        /* Opened as FILE pointers. */
        fclose(writer->fp);
        break;
    case LOG_OT_SYSLOG:
        /* Opened using openlog(). */
        closelog();
        break;
    default:
        /* Nothing opened, or not opened by me, so leave them alone. */
        break;
    }

    LogPrefix *pfx;

    while ((pfx = listRemoveHead(&writer->prefixes)) != NULL) {
        free(pfx->str);
        free(pfx);
    }

    free(writer->host);
    free(writer->file);
    free(writer->separator);

    free(writer);
}

LogChannel *logChannel(const char *name)
{
    LogChannel *chan = calloc(1, sizeof(*chan));

    chan->name = strdup(name);

    return chan;
}

void logConnect(LogChannel *chan, LogWriter *writer)
{
    LogWriterRef *ref = calloc(1, sizeof(*ref));

    ref->writer = writer;

    listAppendTail(&chan->writer_refs, ref);
}

void logCloseChannel(LogChannel *chan)
{
    LogWriterRef *ref;

    listRemove(&channels, chan);

    while ((ref = listRemoveHead(&chan->writer_refs)) != NULL) {
        free(ref);
    }

    free(chan);
}

void _logWrite(LogChannel *chan, const char *file, int line, const char *func, const char *fmt, ...)
{
    Buffer msg = { 0 };

    va_list ap;

    va_start(ap, fmt);
    bufAddV(&msg, fmt, ap);
    va_end(ap);

    for (LogWriterRef *ref = listHead(&chan->writer_refs); ref; ref = listNext(ref)) {
        LogWriter *writer = ref->writer;

        Buffer str = { 0 };

        log_add_prefixes(&writer->prefixes, &str, writer->separator, chan->name, file, line, func);

        bufAdd(&str, bufGet(&msg), bufLen(&msg));

        log_write(writer, bufGet(&str));

        bufClear(&str);
    }

    bufClear(&msg);
}

void logContinue(LogChannel *chan, const char *fmt, ...)
{
    Buffer msg = { 0 };

    va_list ap;

    va_start(ap, fmt);
    bufAddV(&msg, fmt, ap);
    va_end(ap);

    for (LogWriterRef *ref = listHead(&chan->writer_refs); ref; ref = listNext(ref)) {
        LogWriter *writer = ref->writer;

        log_write(writer, bufGet(&msg));
    }

    bufClear(&msg);
}

void logReset(void)
{
    LogChannel *chan, *next_chan;
    LogWriter *writer, *next_writer;

    for (chan = listHead(&channels); chan; chan = next_chan) {
        next_chan = listNext(chan);

        logCloseChannel(chan);
    }

    for (writer = listHead(&writers); writer; writer = next_writer) {
        next_writer = listNext(writer);

        logCloseWriter(writer);
    }
}

#ifdef TEST
static int errors = 0;

static int test_time_format(void)
{
    struct tm tm_local = { 0 };
    struct timeval tv = { 0 };

    log_get_time(&tv);

    gmtime_r(&tv.tv_sec, &tm_local);

    Buffer buf = { 0 };

    log_add_time(&buf, &tm_local, &tv, "%Y-%m-%d %H:%M:%S.%4N: bladibla");
    make_sure_that(strcmp(bufGet(&buf), "1970-01-01 12:34:56.1235: bladibla") == 0);

    bufClear(&buf);

    log_add_time(&buf, &tm_local, &tv, "%6N");
    make_sure_that(strcmp(bufGet(&buf), "123456") == 0);

    bufClear(&buf);

    log_add_time(&buf, &tm_local, &tv, "%0N");
    make_sure_that(strcmp(bufGet(&buf), "") == 0);

    bufClear(&buf);

    log_add_time(&buf, &tm_local, &tv, "%12N");
    make_sure_that(strcmp(bufGet(&buf), "123456") == 0);

    bufClear(&buf);

    return errors;
}

int main(int argc, char *argv[])
{
    errors += test_time_format();

    LogWriter *file_writer = logFileWriter("/tmp/debug.log");

    logWithUniversalTime(file_writer, "%Y-%m-%d/%H:%M:%S.%6N");
    logWithChannel(file_writer);
    logWithFunction(file_writer);
    logWithString(file_writer, "@");
    logWithFile(file_writer);
    logWithString(file_writer, ":");
    logWithLine(file_writer);
    logWithSeparator(file_writer, "\t");

    LogWriter *stderr_writer = logFPWriter(stderr);

    LogChannel *err_channel = logChannel("ERROR");
    LogChannel *info_channel = logChannel("INFO");
    LogChannel *debug_channel = logChannel("DEBUG");

    logConnect(err_channel, file_writer);
    logConnect(err_channel, stderr_writer);

    logConnect(info_channel, file_writer);
    logConnect(debug_channel, file_writer);

    _logWrite(err_channel, "log.c", 1, "func", "This is an error.\n");
    _logWrite(info_channel, "log.c", 2, "func", "This is an info message.\n");
    _logWrite(debug_channel, "log.c", 3, "func", "This is a debug message.\n");

    return errors;
}
#endif
