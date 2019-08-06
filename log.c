/*
 * log.c: Provide logging.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-29
 * Version:   $Id: log.c 341 2019-08-06 12:39:24Z jacco $
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

/*
 * Types of prefixes.
 */
typedef enum {
    LOG_PT_UTIME,                       // UTC time.
    LOG_PT_LTIME,                       // Local time.
    LOG_PT_FILE,                        // File where logWrite was called.
    LOG_PT_LINE,                        // Line where logWrite was called.
    LOG_PT_FUNC,                        // Function where logWrite was called.
    LOG_PT_CHAN,                        // Name of log channel.
    LOG_PT_STR                          // Fixed string.
} LogPrefixType;

/*
 * A prefix definition.
 */
typedef struct {
    ListNode _node;                     // Make it listable.
    LogPrefixType type;                 // Prefix type.
    char *str;                          // String (for UTIME, LTIME and STR).
} LogPrefix;

/*
 * A log channel.
 */
struct LogChannel {
    ListNode _node;                     // Make it listable.
    char *name;                         // Name of the channel.
    List writer_refs;                   // List of writer references.
};

/*
 * Types of writers.
 */
typedef enum {
    LOG_OT_FILE,                        // Write to a named file.
    LOG_OT_FP,                          // Write to a FILE pointer.
    LOG_OT_FD,                          // Write to a file descriptor.
    LOG_OT_TCP,                         // Write on a TCP connection.
    LOG_OT_UDP,                         // Write to a UDP socket.
    LOG_OT_SYSLOG,                      // Write to syslog.
    LOG_OT_FUNC,                        // Write to a handler function.
    LOG_OT_BUFFER                       // Write to a Buffer.
} LogWriterType;

/*
 * A log writer.
 */
struct LogWriter {
    ListNode _node;                     // Make it listable.
    LogWriterType type;                 // Type of this writer.
    List prefixes;                      // List of prefixes to use.
    char *host;                         // Host name (UDP and TCP).
    uint16_t port;                      // Port number (UDP and TCP).
    char *file;                         // File name (FILE).
    FILE *fp;                           // FILE pointer (FILE and FP).
    int fd;                             // File descriptor (FD, UDP, TCP).
    int priority;                       // Priority (SYSLOG).
    void (*handler)(const char *msg, void *udata);
                                        // Handler function (FUNC).
    void *udata;                        // User data (FUNC).
    Buffer *buffer;                     // Buffer (BUFFER).
    char *separator;                    // Field separator.
};

/*
 * A reference to a log writer.
 */
typedef struct {
    ListNode _node;                     // Make it listable.
    LogWriter *writer;                  // Pointer to the writer.
} LogWriterRef;

static List channels = { 0 };           // The channels we know.
static List writers = { 0 };            // The writers we know.

/*
 * Get the current timestamp in <ts>. In case we're running tests return a fixed
 * time that we can test against.
 */
static void log_get_time(struct timespec *ts)
{
#ifdef TEST
    ts->tv_sec  = 12 * 3600 + 34 * 60 + 56;
    ts->tv_nsec = 123456789;
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}

/*
 * Add the timestamp data in <tm> and <ts> to <buf>, using the
 * strftime-compatible format string in <fmt>. This function supports an
 * additional format specifier "%<n>N" which is replaced with <n> sub-second
 * digits (i.e. 2 gives hundredths, 3 gives thousands, 6 gives millionths etc.)
 * %N is a specifier in the date command where it gives nanoseconds. The number
 * of digits can be 0 to 9. The default, if no <n> is given is 9.
 */
static void log_add_time(Buffer *buf, struct tm *tm, struct timespec *ts, const char *fmt)
{
    static regex_t *regex = NULL;

    const size_t nmatch = 2;
    regmatch_t pmatch[nmatch];

    int precision;
    double remainder = (double) ts->tv_nsec / 1000000000.0;
    char rem_string[12]; // "0.123456789\0"

    if (regex == NULL) {
        regex = calloc(1, sizeof(*regex));
        regcomp(regex, "%([+-]?[0-9]*)N", REG_EXTENDED);
    }

    char *new_str = NULL;
    char *cur_str = strdup(fmt);

    while (regexec(regex, cur_str, nmatch, pmatch, 0) == 0) {
        if (sscanf(cur_str + pmatch[1].rm_so, "%d", &precision) == 0) {
            precision = 9;
        }
        else {
            precision = CLAMP(precision, 0, 9);
        }

        snprintf(rem_string, sizeof(rem_string), "%0*.*f", 2 + precision, precision, remainder);

        if (asprintf(&new_str, "%.*s%s%s",
                pmatch[0].rm_so, cur_str, rem_string + 2, cur_str + pmatch[0].rm_eo) == -1) {
            break;
        }

        cur_str = strdup(new_str);

        free(new_str);
    }

    new_str = malloc(80);

    strftime(new_str, 80, cur_str, tm);

    free(cur_str);

    bufAddS(buf, new_str);

    free(new_str);
}

/*
 * Add the prefixes in <prefixes> to <buf>. Use, if necessary, <separator>,
 * <chan>, <file>, <line>, <func> as values.
 */
static void log_add_prefixes(List *prefixes, Buffer *buf,
        const char *separator, const char *chan, const char *file, int line, const char *func)
{
    struct tm tm_local = { 0 }, tm_utc = { 0 };
    static struct timespec ts = { 0 };

    LogPrefix *pfx, *next_pfx;

    ts.tv_sec = -1;
    tm_utc.tm_sec = -1;
    tm_local.tm_sec = -1;

    for (pfx = listHead(prefixes); pfx; pfx = next_pfx) {
        next_pfx = listNext(pfx);

        switch(pfx->type) {
        case LOG_PT_LTIME:
            if (ts.tv_sec == -1) log_get_time(&ts);
            if (tm_local.tm_sec == -1) localtime_r(&ts.tv_sec, &tm_local);

            log_add_time(buf, &tm_local, &ts, pfx->str);

            break;
        case LOG_PT_UTIME:
            if (ts.tv_sec == -1) log_get_time(&ts);
            if (tm_utc.tm_sec == -1) gmtime_r(&ts.tv_sec, &tm_utc);

            log_add_time(buf, &tm_utc, &ts, pfx->str);

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

        // Don't insert a separator if the current or the next prefix is a
        // LOG_PT_STR, and don't append a separator if the prefix list ends on a
        // LOG_PT_STR.

        if (pfx->type != LOG_PT_STR &&
            (next_pfx == NULL || next_pfx->type != LOG_PT_STR)) {
            bufAddS(buf, separator ? separator : " ");
        }
    }
}

/*
 * Write out the log message in <buf> to <writer>.
 */
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
    case LOG_OT_BUFFER:
        bufAddS(writer->buffer, buf);
        break;
    }
}

/*
 * Add an (as yet empty) prefix of type <type> to <writer>.
 */
static LogPrefix *log_add_prefix(LogWriter *writer, LogPrefixType type)
{
    LogPrefix *prefix = calloc(1, sizeof(LogPrefix));

    prefix->type = type;

    listAppendTail(&writer->prefixes, prefix);

    return prefix;
}

/*
 * Add an (as yet empty) log writer of type <type>.
 */
static LogWriter *log_add_writer(LogWriterType type)
{
    LogWriter *writer = calloc(1, sizeof(*writer));

    writer->type = type;

    return writer;
}

/*
 * If there is an existing file writer that writes to <filename> return it,
 * otherwise return NULL.
 */
static LogWriter *log_find_file_writer(const char *filename)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FILE && strcmp(writer->file, filename) == 0) break;
    }

    return writer;
}

/*
 * Add a file writer that writes to <filename>.
 */
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

/*
 * If there is an existing FP writer that writes to <fp> return it, otherwise
 * return NULL.
 */
static LogWriter *log_find_fp_writer(FILE *fp)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FP && writer->fp == fp) break;
    }

    return writer;
}

/*
 * Add an FP writer that writes to <fp>.
 */
static LogWriter *log_add_fp_writer(FILE *fp)
{
    LogWriter *writer = log_add_writer(LOG_OT_FP);

    writer->fp = fp;

    return writer;
}

/*
 * If there is an existing FD writer that writes to <fd> return it, otherwise
 * return NULL.
 */
static LogWriter *log_find_fd_writer(int fd)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FD && writer->fd == fd) break;
    }

    return writer;
}

/*
 * Add an FD writer that writes to <fd>.
 */
static LogWriter *log_add_fd_writer(int fd)
{
    LogWriter *writer = log_add_writer(LOG_OT_FD);

    writer->fd = fd;

    return writer;
}

/*
 * Add a syslog writer that writes messages using <ident>, <option>, <facility>
 * and <priority> (see "man 3 syslog" for details on these parameters).
 */
static LogWriter *log_add_syslog_writer(const char *ident, int option,
        int facility, int priority)
{
    LogWriter *writer = log_add_writer(LOG_OT_SYSLOG);

    openlog(ident, option, facility);

    writer->priority = priority;

    return writer;
}

/*
 * If there is an existing UDP writer that writes to <port> on <host> return
 * it, otherwise return NULL.
 */
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

/*
 * Add a UDP writer that sends log messages as UDP packets to <port> on <host>.
 */
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

/*
 * If there is an existing TCP writer that writes to <port> on <host> return
 * it, otherwise return NULL.
 */
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

/*
 * Add a TCP writer that sends log messages over a TCP connection to <port> on
 * <host>.
 */
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

/*
 * If there is an existing function writer that writes to <handler>, using
 * <udata>, return it, otherwise return NULL.
 */
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

/*
 * Add a log writer that calls <handler> with the log message and the same
 * <udata> that was given here.
 */
static LogWriter *log_add_function_writer(
        void (*handler)(const char *msg, void *udata),
        void *udata)
{
    LogWriter *writer = log_add_writer(LOG_OT_FUNC);

    writer->handler = handler;
    writer->udata = udata;

    return writer;
}

/*
 * If there is an existing buffer writer that writes into <buf> return it,
 * otherwise return NULL.
 */
static LogWriter *log_find_buffer_writer(Buffer *buf)
{
    LogWriter *writer;

    for (writer = listHead(&writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_BUFFER && writer->buffer == buf) break;
    }

    return writer;
}

/*
 * Add a log writer that writes into <buf>.
 */
static LogWriter *log_add_buffer_writer(Buffer *buf)
{
    LogWriter *writer = log_add_writer(LOG_OT_BUFFER);

    writer->buffer = buf;

    return writer;
}

/*
 * Close writer <writer>.
 */
static void log_close_writer(LogWriter *writer)
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

/*
 * Close channel <chan>.
 */
static void log_close_channel(LogChannel *chan)
{
    LogWriterRef *ref;

    listRemove(&channels, chan);

    while ((ref = listRemoveHead(&chan->writer_refs)) != NULL) {
        free(ref);
    }

    free(chan);
}

/*
 * Get a log writer that writes to the file whose name is given by <fmt> and the
 * subsequent parameters.
 */
__attribute__((format (printf, 1, 2)))
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

/*
 * Get a log writer that writes to FILE pointer <fp>.
 */
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

/*
 * Get a log writer that writes to file descriptor <fd>.
 */
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

/*
 * Get a log writer that writes to a TCP connection to <port> at <host>.
 */
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

/*
 * Get a log writer that sends log messages as UDP packets to <port> on <host>.
 */
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

/*
 * Get a log writer that sends log messages to syslog, using the given
 * parameters. See "man 3 syslog" for more information on them.
 */
LogWriter *logSyslogWriter(const char *ident, int option, int facility, int priority)
{
    return log_add_syslog_writer(ident, option, facility, priority);
}

/*
 * Get a log writer that calls <handler> for every log message, passing in the
 * message as <msg> and the same <udata> that was given here.
 */
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

/*
 * Get a log writer that writes into Buffer <buf>.
 */
LogWriter *logBufferWriter(Buffer *buf)
{
    LogWriter *writer;

    if ((writer = log_find_buffer_writer(buf)) == NULL &&
        (writer = log_add_buffer_writer(buf)) == NULL) {
        return NULL;
    }
    else {
        return writer;
    }
}

/*
 * Prefix log messages to <writer> with the local time, formatted using the
 * strftime-compatible format <fmt>. <fmt> supports an additional format
 * specifier "%<n>N" which is replaced with <n> sub-second digits (i.e. 2 gives
 * hundredths, 3 gives thousands, 6 gives millionths etc.) The number of digits
 * can be 0 to 9. If <n> is not given, the default is 9.
 */
void logWithLocalTime(LogWriter *writer, const char *fmt)
{
    LogPrefix *prefix = log_add_prefix(writer, LOG_PT_LTIME);

    prefix->str = strdup(fmt);
}

/*
 * Prefix log messages to <writer> with the UTC time, formatted using the
 * strftime-compatible format <fmt>. <fmt> supports an additional format
 * specifier "%<n>N" which is replaced with <n> sub-second digits (i.e. 2 gives
 * hundredths, 3 gives thousands, 6 gives millionths etc.) The number of digits
 * can be 0 to 9. If <n> is not given, the default is 9.
 */
void logWithUniversalTime(LogWriter *writer, const char *fmt)
{
    LogPrefix *prefix = log_add_prefix(writer, LOG_PT_UTIME);

    prefix->str = strdup(fmt);
}

/*
 * Prefix log messages to <writer> with the name of the source file in which
 * logWrite was called.
 */
void logWithFile(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_FILE);
}

/*
 * Prefix log messages to <writer> with the line on which logWrite was called.
 */
void logWithLine(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_LINE);
}

/*
 * Prefix log messages to <writer> with the name of the function in which
 * logWrite was called.
 */
void logWithFunction(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_FUNC);
}

/*
 * Prefix log messages to <writer> with the name of the channel through which
 * the message is sent.
 */
void logWithChannel(LogWriter *writer)
{
    log_add_prefix(writer, LOG_PT_CHAN);
}

/*
 * Prefix log messages to <writer> with the string defined by the
 * printf-compatible format string <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 2, 3)))
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

/*
 * Separate prefixes in log messages to <writer> with the separator given by
 * <sep>. Separators are *not* written if the prefix before or after it is a
 * predefined string (added using logWithString). The user may be trying to set
 * up alternative separators between certain prefixes and we don't want to mess
 * that up.
 */
void logWithSeparator(LogWriter *writer, const char *sep)
{
    free(writer->separator);

    writer->separator = strdup(sep);
}

/*
 * Create a new log channel with the name <name>.
 */
LogChannel *logChannel(const char *name)
{
    LogChannel *chan = calloc(1, sizeof(*chan));

    chan->name = strdup(name);

    return chan;
}

/*
 * Connect log channel <chan> to writer <writer>.
 */
void logConnect(LogChannel *chan, LogWriter *writer)
{
    LogWriterRef *ref = calloc(1, sizeof(*ref));

    ref->writer = writer;

    listAppendTail(&chan->writer_refs, ref);
}

/*
 * Write a log message to channel <chan>, defined by the printf-compatible
 * format string <fmt> and the subsequent parameters. If necessary, <file>,
 * <line> and <func> will be used to fill the appropriate prefixes.
 *
 * Call this function through the logWrite macro, which will fill in <file>,
 * <line> and <func> for you.
 */
__attribute__((format (printf, 5, 6)))
void _logWrite(LogChannel *chan,
        const char *file, int line, const char *func,
        const char *fmt, ...)
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

/*
 * Continue a previous log message using the printf-compatible format string
 * <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 2, 3)))
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

/*
 * Reset all logging. Deletes all created channels and writers.
 */
void logReset(void)
{
    LogChannel *chan, *next_chan;
    LogWriter *writer, *next_writer;

    for (chan = listHead(&channels); chan; chan = next_chan) {
        next_chan = listNext(chan);

        log_close_channel(chan);
    }

    for (writer = listHead(&writers); writer; writer = next_writer) {
        next_writer = listNext(writer);

        log_close_writer(writer);
    }
}

#ifdef TEST
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int errors = 0;

static int test_time_format(void)
{
    struct tm tm_local = { 0 };
    struct timespec ts = { 0 };

    log_get_time(&ts);

    gmtime_r(&ts.tv_sec, &tm_local);

    Buffer buf = { 0 };

    // Check a general string with some standard strftime specifiers.

    log_add_time(&buf, &tm_local, &ts, "%Y-%m-%d %H:%M:%S.%3N: bladibla");
    make_sure_that(strcmp(bufGet(&buf), "1970-01-01 12:34:56.123: bladibla") == 0);

    bufClear(&buf);

    // Check rounding

    log_add_time(&buf, &tm_local, &ts, "%6N");
    make_sure_that(strcmp(bufGet(&buf), "123457") == 0);

    bufClear(&buf);

    // Check no digits at all

    log_add_time(&buf, &tm_local, &ts, "%0N");
    make_sure_that(strcmp(bufGet(&buf), "") == 0);

    bufClear(&buf);

    // Check too great precision

    log_add_time(&buf, &tm_local, &ts, "%12N");
    make_sure_that(strcmp(bufGet(&buf), "123456789") == 0);

    bufClear(&buf);

    // Check default precision

    log_add_time(&buf, &tm_local, &ts, "%N");
    make_sure_that(strcmp(bufGet(&buf), "123456789") == 0);

    bufClear(&buf);

    // Check multiple specifiers

    log_add_time(&buf, &tm_local, &ts, "%3N/%6N");
    make_sure_that(strcmp(bufGet(&buf), "123/123457") == 0);

    bufClear(&buf);

    // Check that negative digit counts are handled as 0.

    log_add_time(&buf, &tm_local, &ts, "%-3N");
    make_sure_that(strcmp(bufGet(&buf), "") == 0);

    bufClear(&buf);

    // Check that positive digit counts are accepted.

    log_add_time(&buf, &tm_local, &ts, "%+3N");
    make_sure_that(strcmp(bufGet(&buf), "123") == 0);

    bufClear(&buf);

    return errors;
}

#define FILE_WRITER_TEST_FILE "/tmp/test_log_to_file.log"
#define FP_WRITER_TEST_FILE "/tmp/test_log_to_fp.log"
#define FD_WRITER_TEST_FILE "/tmp/test_log_to_fd.log"

static int compare_file(const char *filename, const char *text)
{
    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "Could not open file \"%s\" for comparison.\n", filename);
        return 1;
    }

    struct stat statbuf;

    fstat(fd, &statbuf);

    char *content = calloc(1, statbuf.st_size + 1);

    if (read(fd, content, statbuf.st_size) != statbuf.st_size) {
        fprintf(stderr, "Could not read file \"%s\" for comparison.\n", filename);
        return 1;
    }

    int r = strncmp(content, text, statbuf.st_size);

    if (r != 0) {
        fprintf(stderr, "File \"%s\" does not match expectation.\n", filename);
        fprintf(stderr, "Expected:\n");
        fputs(text, stderr);
        fprintf(stderr, "Actual:\n");
        fputs(content, stderr);
    }

    free(content);

    return r == 0 ? 0 : 1;
}

int main(int argc, char *argv[])
{
    FILE *fp = NULL;
    int   fd = -1;
    LogWriter *file_writer;
    LogWriter *fp_writer;
    LogWriter *fd_writer;
    LogWriter *buf_writer;

    errors += test_time_format();

    LogChannel *err_channel = logChannel("ERROR");
    LogChannel *info_channel = logChannel("INFO");
    LogChannel *debug_channel = logChannel("DEBUG");

    if ((file_writer = logFileWriter(FILE_WRITER_TEST_FILE)) == NULL) {
        fprintf(stderr, "Couldn't open %s.\n", FILE_WRITER_TEST_FILE);
        errors++;
        goto quit;
    }

    logWithUniversalTime(file_writer, "%Y-%m-%d/%H:%M:%S.%6N");
    logWithString(file_writer, " FILE ");
    logWithChannel(file_writer);
    logWithFunction(file_writer);
    logWithString(file_writer, "@");
    logWithFile(file_writer);
    logWithString(file_writer, ":");
    logWithLine(file_writer);

    logConnect(err_channel, file_writer);
    logConnect(info_channel, file_writer);

    if ((fp = fopen(FP_WRITER_TEST_FILE, "w")) == NULL) {
        fprintf(stderr, "Couldn't open %s.\n", FP_WRITER_TEST_FILE);
        errors++;
        goto quit;
    }
    else if ((fp_writer = logFPWriter(fp)) == NULL) {
        fprintf(stderr, "logFPWriter returned NULL.\n");
        errors++;
        goto quit;
    }

    logWithUniversalTime(fp_writer, "%Y-%m-%d/%H:%M:%S.%6N");
    logWithString(fp_writer, "\tFP\t");
    logWithChannel(fp_writer);
    logWithFunction(fp_writer);
    logWithString(fp_writer, "@");
    logWithFile(fp_writer);
    logWithString(fp_writer, ":");
    logWithLine(fp_writer);
    logWithSeparator(fp_writer, "\t");

    logConnect(err_channel, fp_writer);
    logConnect(debug_channel, fp_writer);

    if ((fd = creat(FD_WRITER_TEST_FILE, 0666)) == -1) {
        fprintf(stderr, "Couldn't open %s.\n", FD_WRITER_TEST_FILE);
        errors++;
        goto quit;
    }
    else if ((fd_writer = logFDWriter(fd)) == NULL) {
        fprintf(stderr, "logFDWriter returned NULL.\n");
        errors++;
        goto quit;
    }

    logWithUniversalTime(fd_writer, "%Y-%m-%d/%H:%M:%S.%6N");
    logWithString(fd_writer, ",FD,");
    logWithChannel(fd_writer);
    logWithFunction(fd_writer);
    logWithString(fd_writer, "@");
    logWithFile(fd_writer);
    logWithString(fd_writer, ":");
    logWithLine(fd_writer);
    logWithSeparator(fd_writer, ",");

    logConnect(info_channel, fd_writer);
    logConnect(debug_channel, fd_writer);

    Buffer log_buffer = { 0 };

    if ((buf_writer = logBufferWriter(&log_buffer)) == NULL) {
        fprintf(stderr, "logBufferWriter returned NULL.\n");
        errors++;
        goto quit;
    }

    logWithUniversalTime(buf_writer, "%Y-%m-%d/%H:%M:%S.%6N");
    logWithString(buf_writer, " - BUFFER - ");
    logWithChannel(buf_writer);
    logWithFunction(buf_writer);
    logWithString(buf_writer, "@");
    logWithFile(buf_writer);
    logWithString(buf_writer, ":");
    logWithLine(buf_writer);
    logWithSeparator(buf_writer, " - ");

    logConnect(debug_channel, buf_writer);

    _logWrite(err_channel, "log.c", 1, "func", "This is an error.\n");
    _logWrite(info_channel, "log.c", 2, "func", "This is an info message.\n");
    _logWrite(debug_channel, "log.c", 3, "func", "This is a debug message.\n");

    logReset();

    if (fp != NULL) { fclose(fp); fp = NULL; }
    if (fd >= 0)    { close(fd);  fd = -1; }

    errors += compare_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.123457 FILE ERROR func@log.c:1 This is an error.\n"
            "1970-01-01/12:34:56.123457 FILE INFO func@log.c:2 This is an info message.\n");

    errors += compare_file(FP_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.123457\tFP\tERROR\tfunc@log.c:1\tThis is an error.\n"
            "1970-01-01/12:34:56.123457\tFP\tDEBUG\tfunc@log.c:3\tThis is a debug message.\n");

    errors += compare_file(FD_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.123457,FD,INFO,func@log.c:2,This is an info message.\n"
            "1970-01-01/12:34:56.123457,FD,DEBUG,func@log.c:3,This is a debug message.\n");

    errors += strcmp(bufGet(&log_buffer),
            "1970-01-01/12:34:56.123457 - BUFFER - DEBUG - "
            "func@log.c:3 - This is a debug message.\n") != 0;

quit:
    logReset();

    if (fp != NULL) { fclose(fp); fp = NULL; }
    if (fd >= 0)    { close(fd);  fd = -1; }

    if (access(FILE_WRITER_TEST_FILE, F_OK) == 0) {
        unlink(FILE_WRITER_TEST_FILE);
    }

    if (access(FP_WRITER_TEST_FILE, F_OK) == 0) {
        unlink(FP_WRITER_TEST_FILE);
    }

    if (access(FD_WRITER_TEST_FILE, F_OK) == 0) {
        unlink(FD_WRITER_TEST_FILE);
    }

    return errors;
}
#endif
