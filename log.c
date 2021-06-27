/*
 * log.c: Provide logging.
 *
 * This log package works with log "channels", which are connected to log
 * "writers". You write log messages to one or more channels, and the connected
 * log writers take care of writing them, with any number of prefixes of your
 * choosing, to your chosen destination. Writing to a channel that is not
 * connected to a writer is effectively a no-op: the log message will disappear
 * into the void.
 *
 * A log channel identifier is a single bit in a 64-bit bitmask (which means
 * there are 64 channels in total). If a function has "uint64_t channels" as a
 * parameter you can pass in a bitmask with multiple bits set, and the function
 * will apply to all the associated channels.
 *
 * Log writers take care of writing the log messages to a given destination.
 * There are log writers that write to files, file pointers, file descriptors,
 * Buffers (see buf.h) and the syslog facility. There is also a log writer that
 * calls a user-supplied function for each log message.
 *
 * Each log writer can have a number of associated prefixes that are written out
 * before the actual log message (in the order in which they were requested).
 * You can have the current date and time, the file, line and function from
 * which the log message was written, and also any fixed string. You can also
 * set a separator between the fields of the log message.
 *
 * log.c is part of libjvs.
 *
 * Copyright: (c) 2019-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-29
 * Version:   $Id: log.c 423 2021-06-27 12:50:46Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "log.h"

#include "list.h"
#include "pa.h"
#include "net.h"
#include "tcp.h"
#include "udp.h"
#include "buffer.h"
#include "defs.h"

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
    Buffer *buffer;                     // Buffer (BUF).
    char *separator;                    // Field separator.
};

/*
 * A reference to a log writer.
 */
typedef struct {
    ListNode _node;                     // Make it listable.
    LogWriter *writer;                  // Pointer to the writer.
} LogWriterRef;

/*
 * Data for a log channel.
 */
typedef struct {
    List writer_refs;
} LogChannel;

static PointerArray log_channels = { 0 };   // The channels (max. 64) we know.
static List log_writers = { 0 };            // The writers we know.

static const int max_channels = 8 * sizeof(uint64_t);

/*
 * Get the current timestamp in <ts>. In case we're running tests return a fixed
 * time that we can test against.
 */
static void log_get_time(struct timespec *ts)
{
#ifdef TEST
    ts->tv_sec  = 12 * 3600 + 34 * 60 + 56;
    ts->tv_nsec = 987654321;
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}

/*
 * Add the timestamp data in <tm> and <ts> to <buf>, using the
 * strftime-compatible format string in <fmt>. This function also supports an
 * additional digit (and only one digit) in the %S specifier, which gives the
 * number of sub-second digits to follow the decimal point. If <n> is left out
 * altogether, the result is identical to the usual "%S" specifier, where the
 * current seconds value is shown. If <n> is given but 0, the seconds value is
 * rounded to the nearest whole second.
 */
static void log_add_time(Buffer *buf,
        struct tm *tm, struct timespec *ts, const char *fmt)
{
    static regex_t *regex = NULL;

    const size_t nmatch = 2;
    regmatch_t pmatch[nmatch];

    int precision;
    char sec_string[12]; // "0.987654321\0"

    char *new_str = NULL;
    char *cur_str = strdup(fmt);

    if (regex == NULL) {
        regex = calloc(1, sizeof(*regex));
        regcomp(regex, "%([0-9])S", REG_EXTENDED);
    }

    while (regexec(regex, cur_str, nmatch, pmatch, 0) == 0) {
        sscanf(cur_str + pmatch[1].rm_so, "%d", &precision);

        int field_width = precision == 0 ? 2 : 3 + precision;

        snprintf(sec_string, sizeof(sec_string), "%0*.*f",
                field_width, precision,
                tm->tm_sec + ts->tv_nsec / 1000000000.0);

        if (asprintf(&new_str, "%.*s%s%s",
                pmatch[0].rm_so, cur_str,
                sec_string,
                cur_str + pmatch[0].rm_eo) == -1) {
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
        const char *separator, const char *file, int line, const char *func)
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
        fflush(writer->fp);
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

    for (writer = listHead(&log_writers); writer; writer = listNext(writer)) {
        if (writer->type == LOG_OT_FILE &&
            strcmp(writer->file, filename) == 0) break;
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

    for (writer = listHead(&log_writers); writer; writer = listNext(writer)) {
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

    for (writer = listHead(&log_writers); writer; writer = listNext(writer)) {
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

    for (writer = listHead(&log_writers); writer; writer = listNext(writer)) {
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

    if ((fd = udpSocket()) < 0 || udpConnect(fd, host, port) != 0) {
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

    for (writer = listHead(&log_writers); writer; writer = listNext(writer)) {
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

    for (writer = listHead(&log_writers); writer; writer = listNext(writer)) {
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

    for (writer = listHead(&log_writers); writer; writer = listNext(writer)) {
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
    // First remove it from all channels that write to it.

    for (int i = 0; i < max_channels; i++) {
        LogWriterRef *ref, *next_ref;
        LogChannel *chan;

        if ((chan = paGet(&log_channels, i)) == NULL) continue;

        for (ref = listHead(&chan->writer_refs); ref; ref = next_ref) {
            next_ref = listNext(ref);

            if (ref->writer == writer) {
                listRemove(&chan->writer_refs, ref);
                free(ref);
            }
        }
    }

    // Then close it.

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

    // Remove all prefix definitions.

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
LogWriter *logSyslogWriter(const char *ident, int option,
        int facility, int priority)
{
    return log_add_syslog_writer(ident, option, facility, priority);
}

/*
 * Get a log writer that calls <handler> for every log message, passing in the
 * message as <msg> and the same <udata> that was given here.
 */
LogWriter *logFunctionWriter(void (*handler)(const char *msg, void *udata),
        void *udata)
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
 * strftime-compatible format <fmt>. <fmt> supports an additional digit in the
 * %S specifier (i.e. "%<n>S") where <n> is the number of sub-second digits to
 * add after the decimal point. If <n> is left out altogether, the result is
 * identical to the usual "%S" specifier, where the current seconds value is
 * shown. If <n> is 0, the seconds value is rounded to the nearest whole second.
 */
void logWithLocalTime(LogWriter *writer, const char *fmt)
{
    LogPrefix *prefix = log_add_prefix(writer, LOG_PT_LTIME);

    prefix->str = strdup(fmt);
}

/*
 * Prefix log messages to <writer> with the UTC time, formatted using the
 * strftime-compatible format <fmt>. <fmt> supports an additional digit in the
 * %S specifier (i.e. "%<n>S") where <n> is the number of sub-second digits to
 * add after the decimal point. If <n> is left out altogether, the result is
 * identical to the usual "%S" specifier, where the current seconds value is
 * shown. If <n> is 0, the seconds value is rounded to the nearest whole second.
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
 * string prefix (added using logWithString). The user is probably trying to set
 * up alternative separators between certain prefixes and we don't want to mess
 * that up. The default separator is a single space.
 */
void logWithSeparator(LogWriter *writer, const char *sep)
{
    free(writer->separator);

    writer->separator = strdup(sep);
}

/*
 * Connect the log channels in <channels> to writer <writer>.
 */
void logConnect(uint64_t channels, LogWriter *writer)
{
    for (int i = 0; i < max_channels; i++) {
        if ((channels & (1ULL << i)) == 0) continue;

        LogChannel *chan;

        if ((chan = paGet(&log_channels, i)) == NULL) {
            chan = calloc(1, sizeof(*chan));
            paSet(&log_channels, i, chan);
        }

        LogWriterRef *ref = calloc(1, sizeof(*ref));

        ref->writer = writer;

        listAppendTail(&chan->writer_refs, ref);
    }
}

/*
 * Write a log message to <channels>, defined by the printf-compatible
 * format string <fmt> and the subsequent parameters. If <with_prefixes> is
 * true, the message will be preceded by the requested prefixes. <file>,
 * <line> and <func> will be used to fill the appropriate prefixes, if
 * necessary.
 *
 * Call this function through the logWrite macro, which will fill in <file>,
 * <line> and <func> for you.
 */
__attribute__((format (printf, 6, 7)))
void _logWrite(uint64_t channels, bool with_prefixes,
        const char *file, int line, const char *func,
        const char *fmt, ...)
{
    Buffer *msg = NULL;

    for (int i = 0; i < max_channels; i++) {
        LogChannel *chan;
        LogWriterRef *ref;

        if (((1ULL << i) & channels) == 0     ||    // Don't want to write here
            !(chan = paGet(&log_channels, i)) ||    // Don't know this channel
            listIsEmpty(&chan->writer_refs))        // Channel is unconnected
            continue;                               // (shouldn't happen)

        // Only build the message if we have to...

        if (msg == NULL) {
            msg = bufCreate();

            va_list ap;

            va_start(ap, fmt);
            bufAddV(msg, fmt, ap);
            va_end(ap);
        }

        // Send the message out to all connected writers.

        for (ref = listHead(&chan->writer_refs); ref; ref = listNext(ref)) {
            LogWriter *writer = ref->writer;

            if (with_prefixes) {
                Buffer str = { 0 };

                log_add_prefixes(&writer->prefixes, &str, writer->separator,
                        file, line, func);

                bufAdd(&str, bufGet(msg), bufLen(msg));

                log_write(writer, bufGet(&str));

                bufClear(&str);
            }
            else {
                log_write(writer, bufGet(msg));
            }
        }
    }

    if (msg) bufDestroy(msg);
}

/*
 * Reset all logging. Disconnects all channels and deletes all writers.
 */
void logReset(void)
{
    LogWriter *writer, *next_writer;

    for (writer = listHead(&log_writers); writer; writer = next_writer) {
        next_writer = listNext(writer);

        log_close_writer(writer);
    }

    for (int i = 0; i < max_channels; i++) {
        LogChannel *chan = paGet(&log_channels, i);

        if (chan) {
            paDrop(&log_channels, i);

            free(chan);
        }
    }
}

#ifdef TEST
#include "utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int errors = 0;

#define check_string(s1, s2) _check_string(__FILE__, __LINE__, s1, s2);

/*
 * Check that strings <s1> and <s2> are identical. Complain and return 1 if
 * they're not, otherwise return 0.
 */
static int _check_string(const char *file, int line,
        const char *s1, const char *s2)
{
    if (strcmp(s1, s2) != 0) {
        fprintf(stderr, "%s:%d: String does not match expectation.\n",
                file, line);
        fprintf(stderr, "Expected:\n");
        fprintf(stderr, "<%s>\n", s1);
        fprintf(stderr, "Actual:\n");
        fprintf(stderr, "<%s>\n", s2);

        errors++;

        return 1;
    }

    return 0;
}

/*
 * Test the extended time format.
 */
static int test_time_format(void)
{
    struct tm tm_local = { 0 };
    struct timespec ts = { 0 };

    log_get_time(&ts);

    gmtime_r(&ts.tv_sec, &tm_local);

    Buffer buf = { 0 };

    // Check a general string with some standard strftime specifiers.

    log_add_time(&buf, &tm_local, &ts, "%Y-%m-%d %H:%M:%3S: bladibla");
    check_string(bufGet(&buf), "1970-01-01 12:34:56.988: bladibla");

    bufClear(&buf);

    // Check rounding

    log_add_time(&buf, &tm_local, &ts, "%6S");
    check_string(bufGet(&buf), "56.987654");

    bufClear(&buf);

    // Check default format

    log_add_time(&buf, &tm_local, &ts, "%S");
    check_string(bufGet(&buf), "56");

    bufClear(&buf);

    // Check no digits at all

    log_add_time(&buf, &tm_local, &ts, "%0S");
    check_string(bufGet(&buf), "57");

    bufClear(&buf);

    // Check multiple specifiers

    log_add_time(&buf, &tm_local, &ts, "%3S/%6S");
    check_string(bufGet(&buf), "56.988/56.987654");

    bufClear(&buf);

    return errors;
}

#define FILE_WRITER_TEST_FILE "/tmp/test_log_to_file.log"
#define FP_WRITER_TEST_FILE "/tmp/test_log_to_fp.log"
#define FD_WRITER_TEST_FILE "/tmp/test_log_to_fd.log"

/*
 * Check that the file with <filename> contains exactly <text>. Complain and
 * return 1 if it doesn't, otherwise return 0.
 */
static int check_file(const char *filename, const char *text)
{
    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr,
                "Could not open file \"%s\" for comparison.\n", filename);
        return 1;
    }

    struct stat statbuf;

    fstat(fd, &statbuf);

    char *content = calloc(1, statbuf.st_size + 1);

    if (read(fd, content, statbuf.st_size) != statbuf.st_size) {
        fprintf(stderr,
                "Could not read file \"%s\" for comparison.\n", filename);
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

int main(void)
{
    FILE *fp = NULL;
    int   fd = -1;

    LogWriter *file_writer;
    LogWriter *fp_writer;
    LogWriter *fd_writer;
    LogWriter *buf_writer;

    // First test the extended time format.
    errors += test_time_format();

    // Open a writer to file FILE_WRITER_TEST_FILE...
    if ((file_writer = logFileWriter(FILE_WRITER_TEST_FILE)) == NULL) {
        fprintf(stderr, "Couldn't open %s.\n", FILE_WRITER_TEST_FILE);
        errors++;
        goto quit;
    }

    // Set some attributes for this writer...
    logWithUniversalTime(file_writer, "%Y-%m-%d/%H:%M:%6S");
    logWithString(file_writer, " FILE ");
    logWithFunction(file_writer);
    logWithString(file_writer, "@");
    logWithFile(file_writer);
    logWithString(file_writer, ":");
    logWithLine(file_writer);

    // Connect channel CH_ERR to it...
    logConnect(CH_ERR, file_writer);

    // And write a log message to CH_ERR.
    _logWrite(CH_ERR, true, "log.c", 1, "func", "This is an error.\n");

    // Check that the correct entry was made in the log file.
    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 "
            "This is an error.\n");

    // Open a file, and create a writer to the associated FILE pointer.
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

    // Attributes...
    logWithUniversalTime(fp_writer, "%Y-%m-%d/%H:%M:%6S");
    logWithString(fp_writer, "\tFP\t");
    logWithFunction(fp_writer);
    logWithString(fp_writer, "@");
    logWithFile(fp_writer);
    logWithString(fp_writer, ":");
    logWithLine(fp_writer);
    logWithSeparator(fp_writer, "\t");

    // Connect CH_INFO...
    logConnect(CH_INFO, fp_writer);

    // And write a message.
    _logWrite(CH_INFO, true, "log.c", 2, "func", "This is an info message.\n");

    // Make sure the correct message was written, and that the earlier log file
    // hasn't changed.
    errors += check_file(FP_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654\tFP\tfunc@log.c:2\t"
            "This is an info message.\n");

    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 "
            "This is an error.\n");

    // Same thing for a writer to a file descriptor.
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

    logWithUniversalTime(fd_writer, "%Y-%m-%d/%H:%M:%6S");
    logWithString(fd_writer, ",FD,");
    logWithFunction(fd_writer);
    logWithString(fd_writer, "@");
    logWithFile(fd_writer);
    logWithString(fd_writer, ":");
    logWithLine(fd_writer);
    logWithSeparator(fd_writer, ",");

    logConnect(CH_DEBUG, fd_writer);

    _logWrite(CH_DEBUG, true, "log.c", 3, "func", "This is a debug message.\n");

    // Again, check the log file we just made and the earlier ones.
    errors += check_file(FD_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654,FD,func@log.c:3,"
            "This is a debug message.\n");

    errors += check_file(FP_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654\tFP\tfunc@log.c:2\t"
            "This is an info message.\n");

    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 "
            "This is an error.\n");

    // Finally create a writer to a Buffer.
    Buffer log_buffer = { 0 };

    if ((buf_writer = logBufferWriter(&log_buffer)) == NULL) {
        fprintf(stderr, "logBufferWriter returned NULL.\n");
        errors++;
        goto quit;
    }

    // We'll keep this one simple. Just a prefix.
    logWithString(buf_writer, "DEBUG: ");

    // Connect it to CH_DEBUG, which means this channel is now connected to this
    // writer and also to the earlier FD writer!
    logConnect(CH_DEBUG, buf_writer);

    // Write a message to CH_DEBUG...
    _logWrite(CH_DEBUG, true, "log.c", 4, "func",
            "This is another debug message.\n");

    // Make sure it ended up in the log buffer...
    errors += check_string(bufGet(&log_buffer),
            "DEBUG: This is another debug message.\n");

    // And also in the FD writer.
    errors += check_file(FD_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654,FD,func@log.c:3,"
            "This is a debug message.\n"
            "1970-01-01/12:34:56.987654,FD,func@log.c:4,"
            "This is another debug message.\n");

    // And make sure the others haven't changed.
    errors += check_file(FP_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654\tFP\tfunc@log.c:2\t"
            "This is an info message.\n");

    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 "
            "This is an error.\n");

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
