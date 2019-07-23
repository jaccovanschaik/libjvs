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
    LOG_OT_SYSLOG,
    LOG_OT_FUNCTION
} LOG_OutputType;

/*
 * An output stream for logging.
 */
typedef struct {
    ListNode _node;
    LOG_OutputType type;
    uint64_t streams;
    char *file;
    FILE *fp;                   /* For LOG_OT_FILE, LOG_OT_FP */
    int fd;                     /* For LOG_OT_UDP, LOG_OT_TCP, LOG_OT_FD */
    int priority;               /* For LOG_OT_SYSLOG */
    void (*handler)(const char *msg, void *udata);
    void *udata;
} LOG_Output;

/*
 * Prefix types.
 */
typedef enum {
    LOG_PT_UDATE,
    LOG_PT_UTIME,
    LOG_PT_LDATE,
    LOG_PT_LTIME,
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

struct Logger {
    char *separator;

    int stream_count;

    List outputs;                   /* List of outputs. */
    pthread_mutex_t outputs_mutex;  /* Guard multithread access to this list. */

    List prefixes;                  /* List of prefixes. */
    pthread_mutex_t prefixes_mutex; /* Guard multithread access to this list. */
};

/*
 * Add and return a logging output of type <type> on <streams>.
 */
static LOG_Output *log_create_output(Logger *logger, LOG_OutputType type, uint64_t streams)
{
    LOG_Output *out = calloc(1, sizeof(LOG_Output));

    out->type = type;
    out->streams = streams;

    pthread_mutex_lock(&logger->outputs_mutex);

    listAppendTail(&logger->outputs, out);

    pthread_mutex_unlock(&logger->outputs_mutex);

    return out;
}

/*
 * Add a prefix of type <type> to the output.
 */
static LOG_Prefix *log_add_prefix(Logger *logger, LOG_PrefixType type)
{
    LOG_Prefix *prefix = calloc(1, sizeof(LOG_Prefix));

    prefix->type = type;

    pthread_mutex_lock(&logger->prefixes_mutex);

    listAppendTail(&logger->prefixes, prefix);

    pthread_mutex_unlock(&logger->prefixes_mutex);

    return prefix;
}

/*
 * Fill <tm> and <tv> with the current time.
 */
static void log_get_time(struct timeval *tv)
{
#ifdef TEST
    tv->tv_sec  = 12 * 60 * 60;
    tv->tv_usec = 0;
#else
    gettimeofday(tv, NULL);
#endif
}

/*
 * Add the time in <tm> and <tv>, using <precision> sub-second digits, to <buf>.
 */
static void log_add_time(Buffer *buf, struct tm *tm, struct timeval *tv, int precision)
{
    bufAddF(buf, "%02d:%02d:", tm->tm_hour, tm->tm_min);

    if (precision == 0) {
        bufAddF(buf, "%02d", tm->tm_sec);
    }
    else {
        double seconds = (double) tm->tm_sec + (double) tv->tv_usec / 1000000.0;

        bufAddF(buf, "%0*.*f", 3 + precision, precision, seconds);
    }
}

/*
 * Add the date in <tm> to <buf>.
 */
static void log_add_date(Buffer *buf, struct tm *tm)
{
    bufAddF(buf, "%04d-%02d-%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
}

/*
 * Create a new logger.
 */
Logger *logCreate(void)
{
    Logger *logger = calloc(1, sizeof(Logger));

    pthread_mutex_init(&logger->outputs_mutex, NULL);
    pthread_mutex_init(&logger->prefixes_mutex, NULL);

    return logger;
}

/*
 * Open a new stream with name <name> on logger <logger>.
 *
 * Returns a bit mask representing this output stream (or 0 in case no new
 * stream could be opened); these bitmasks can be or-ed together to have
 * multiple streams write to the same output (in the logTo... calls), to add the
 * same prefix to multiple streams (in the logWith... calls), or to write to
 * multiple streams at once (in the logWrite call).
 *
 * A maximum of 64 streams can be opened in a single logger.
 */
uint64_t logStream(Logger *logger, const char *name)
{
    uint64_t stream;

    if (logger->stream_count == 8 * sizeof(uint64_t)) {
        return 0;
    }
    else {
        stream = 1 << logger->stream_count;
        logger->stream_count++;
    }

    return stream;
}

/*
 * Send logging on any of <streams> of <logger> to a UDP socket on port <port>
 * on host <host>. If the host could not be found, -1 is returned and the stream
 * is not created.
 */
int logToUDP(Logger *logger, uint64_t streams, const char *host, uint16_t port)
{
    int fd;

    if ((fd = udpSocket()) < 0 || netConnect(fd, host, port) != 0)
        return -1;
    else {
        LOG_Output *out = log_create_output(logger, LOG_OT_UDP, streams);

        out->fd = fd;

        return 0;
    }
}

/*
 * Send logging on any of <streams> of <logger> through a TCP connection to port
 * <port> on host <host>. If no connection could be opened, -1 is returned and
 * the stream is not created.
 */
int logToTCP(Logger *logger, uint64_t streams, const char *host, uint16_t port)
{
    int fd;

    if ((fd = tcpConnect(host, port)) < 0)
        return -1;
    else {
        LOG_Output *out = log_create_output(logger, LOG_OT_TCP, streams);

        out->fd = fd;

        return 0;
    }
}

/*
 * Write logging on any of <streams> of <logger> to the file whose name is
 * specified with <fmt> and the subsequent parameters. If the file could not be
 * opened, -1 is returned and the stream is not created.
 */
int logToFile(Logger *logger, uint64_t streams, const char *fmt, ...)
{
    va_list ap;

    FILE *fp;
    Buffer filename = { 0 };

    va_start(ap, fmt);
    bufSetV(&filename, fmt, ap);
    va_end(ap);

    if ((fp = fopen(bufGet(&filename), "w")) == NULL) {
        bufClear(&filename);

        return -1;
    }
    else {
        LOG_Output *out = log_create_output(logger, LOG_OT_FILE, streams);

        out->fp = fp;

        bufClear(&filename);

        return 0;
    }
}

/*
 * Write logging on any of <streams> of <logger> to the previously opened FILE
 * pointer <fp>.
 */
int logToFP(Logger *logger, uint64_t streams, FILE *fp)
{
    LOG_Output *out = log_create_output(logger, LOG_OT_FP, streams);

    out->fp = fp;

    return 0;
}

/*
 * Write logging on any of <streams> of <logger> to the previously opened file
 * descriptor <fd>.
 */
int logToFD(Logger *logger, uint64_t streams, int fd)
{
    LOG_Output *out = log_create_output(logger, LOG_OT_FD, streams);

    out->fd = fd;

    return 0;
}

/*
 * Write logging on any of <streams> of <logger> to the syslog facility using
 * the given parameters (see openlog(3) for the meaning of these parameters).
 */
int logToSyslog(Logger *logger, uint64_t streams, const char *ident, int option, int facility, int priority)
{
    LOG_Output *out = log_create_output(logger, LOG_OT_SYSLOG, streams);

    openlog(ident, option, facility);

    out->priority = priority;

    return 0;
}

/*
 * Call function <handler> for every log messageof <logger> , passing in the
 * message and the <udata> that is given here.
 */
int logToFunction(Logger *logger, uint64_t streams, void (*handler)(const char *msg, void *udata), void *udata)
{
    LOG_Output *out = log_create_output(logger, LOG_OT_FUNCTION, streams);

    out->handler = handler;
    out->udata = udata;

    return 0;
}

/*
 * Add a UTC date of the form YYYY-MM-DD in log messages on <logger>.
 */
void logWithUniversalDate(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_UDATE);
}

/*
 * Add a UTC timestamp of the form HH:MM:SS to log messages on <logger>.
 * <precision> is the number of sub-second digits to show.
 */
void logWithUniversalTime(Logger *logger, int precision)
{
    LOG_Prefix *prefix = log_add_prefix(logger, LOG_PT_UTIME);

    prefix->u.precision = CLAMP(precision, 0, 6);
}

/*
 * Add a local date of the form YYYY-MM-DD in log messages on <logger>.
 */
void logWithLocalDate(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_LDATE);
}

/*
 * Add a local timestamp of the form HH:MM:SS to log messages on <logger>.
 * <precision> is the number of sub-second digits to show.
 */
void logWithLocalTime(Logger *logger, int precision)
{
    LOG_Prefix *prefix = log_add_prefix(logger, LOG_PT_LTIME);

    prefix->u.precision = CLAMP(precision, 0, 6);
}

/*
 * Add the name of the file from which the logWrite function was called to log
 * messages on <logger>.
 */
void logWithFile(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_FILE);
}

/*
 * Add the name of the function that called the logWrite function to log
 * messages on <logger>.
 */
void logWithFunction(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_FUNC);
}

/*
 * Add the line number of the call to the logWrite function to log message on
 * <logger>.
 */
void logWithLine(Logger *logger)
{
    log_add_prefix(logger, LOG_PT_LINE);
}

/*
 * Add a string defined by <fmt> and the subsequent arguments to log messages on
 * <logger>.
 */
void logWithString(Logger *logger, const char *fmt, ...)
{
    va_list ap;
    Buffer string = { 0 };

    LOG_Prefix *prefix = log_add_prefix(logger, LOG_PT_STR);

    va_start(ap, fmt);
    bufSetV(&string, fmt, ap);
    va_end(ap);

    prefix->u.string = bufDetach(&string);
}

/*
 * Use a string defined with <fmt> and the subsequent arguments as a separator
 * between the fields of log messages on <logger> (default is a single space).
 */
void logWithSeparator(Logger *logger, const char *fmt, ...)
{
    va_list ap;
    Buffer string = { 0 };

    va_start(ap, fmt);
    bufSetV(&string, fmt, ap);
    va_end(ap);

    if (logger->separator != NULL) {
        free(logger->separator);
    }

    logger->separator = bufDetach(&string);
}

/*
 * Add the requested prefixes to <buf>.
 */
static void log_add_prefixes(Logger *logger, Buffer *buf, const char *file, int line, const char *func)
{
    struct tm tm_local = { 0 }, tm_utc = { 0 };
    static struct timeval tv = { 0 };

    LOG_Prefix *pfx, *next_pfx;

    tv.tv_sec = -1;
    tm_utc.tm_sec = -1;
    tm_local.tm_sec = -1;

    if (logger->separator == NULL) {
        logger->separator = strdup(" ");
    }

    for (pfx = listHead(&logger->prefixes); pfx; pfx = next_pfx) {
        next_pfx = listNext(pfx);

        switch(pfx->type) {
        case LOG_PT_LDATE:
            if (tv.tv_sec == -1) log_get_time(&tv);
            if (tm_local.tm_sec == -1) localtime_r(&tv.tv_sec, &tm_local);

            log_add_date(buf, &tm_local);

            break;
        case LOG_PT_UDATE:
            if (tv.tv_sec == -1) log_get_time(&tv);
            if (tm_utc.tm_sec == -1) gmtime_r(&tv.tv_sec, &tm_utc);

            log_add_date(buf, &tm_utc);

            break;
        case LOG_PT_LTIME:
            if (tv.tv_sec == -1) log_get_time(&tv);
            if (tm_local.tm_sec == -1) localtime_r(&tv.tv_sec, &tm_local);

            log_add_time(buf, &tm_local, &tv, pfx->u.precision);

            break;
        case LOG_PT_UTIME:
            if (tv.tv_sec == -1) log_get_time(&tv);
            if (tm_utc.tm_sec == -1) gmtime_r(&tv.tv_sec, &tm_utc);

            log_add_time(buf, &tm_utc, &tv, pfx->u.precision);

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
            bufAddF(buf, "%s", pfx->u.string);
            break;
        }

        if (pfx->type != LOG_PT_STR &&
                (next_pfx == NULL || next_pfx->type != LOG_PT_STR)) {
            bufAddS(buf, logger->separator);
        }
    }
}

/*
 * Write the log message in <buf> out to <streams>.
 */
static void log_write(Logger *logger, Buffer *buf, uint64_t streams)
{
    LOG_Output *out;

    for (out = listHead(&logger->outputs); out; out = listNext(out)) {
        if ((streams & out->streams) == 0) continue;

        switch(out->type) {
        case LOG_OT_UDP:
        case LOG_OT_TCP:
        case LOG_OT_FD:
            tcpWrite(out->fd, bufGet(buf),
                    bufLen(buf));
            break;
        case LOG_OT_FILE:
        case LOG_OT_FP:
            fwrite(bufGet(buf),
                    bufLen(buf), 1, out->fp);
            fflush(out->fp);
            break;
        case LOG_OT_SYSLOG:
            syslog(out->priority, "%s", bufGet(buf));
            break;
        case LOG_OT_FUNCTION:
            out->handler(bufGet(buf), out->udata);
            break;
        }
    }
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
        close(out->fd);
        break;
    case LOG_OT_FILE:
        /* Opened as FILE pointers. */
        fclose(out->fp);
        break;
    case LOG_OT_SYSLOG:
        /* Opened using openlog(). */
        closelog();
        break;
    default:
        /* Nothing opened, or not opened by me, so leave them alone. */
        break;
    }
}

/*
 * Send out a logging message using <fmt> and the subsequent parameters to
 * <streams> on <logger>. <file>, <line> and <func> are filled in by the
 * logWrite macro, which should be used to call this function.
 */
void _logWrite(Logger *logger, uint64_t streams,
        const char *file, int line, const char *func, const char *fmt, ...)
{
    Buffer scratch = { 0 };

    va_list ap;

    log_add_prefixes(logger, &scratch, file, line, func);

    va_start(ap, fmt);
    bufAddV(&scratch, fmt, ap);
    va_end(ap);

    log_write(logger, &scratch, streams);

    bufClear(&scratch);
}

/*
 * Send a logging message using <fmt> and the subsequent parameters to <streams>
 * on <logger>, *without* any prefixes. Useful to continue a previous log
 * message.
 */
void logContinue(Logger *logger, uint64_t streams, const char *fmt, ...)
{
    Buffer scratch = { 0 };

    va_list ap;

    va_start(ap, fmt);
    bufAddV(&scratch, fmt, ap);
    va_end(ap);

    log_write(logger, &scratch, streams);

    bufClear(&scratch);
}

/*
 * Close <logger>: close all outputs, remove all prefixes.
 */
void logClose(Logger *logger)
{
    LOG_Output *out;
    LOG_Prefix *pfx;

    pthread_mutex_lock(&logger->outputs_mutex);

    while ((out = listRemoveHead(&logger->outputs)) != NULL) {
        log_close_output(out);

        free(out);
    }

    pthread_mutex_unlock(&logger->outputs_mutex);

    pthread_mutex_lock(&logger->prefixes_mutex);

    while ((pfx = listRemoveHead(&logger->prefixes)) != NULL) {
        if (pfx->type == LOG_PT_STR && pfx->u.string != NULL) {
            free(pfx->u.string);
        }

        free(pfx);
    }

    pthread_mutex_unlock(&logger->prefixes_mutex);
}

#ifdef TEST
#include <sys/stat.h>

static int errors = 0;
static Logger *logger = NULL;
static uint64_t debug_stream = 0;
static uint64_t info_stream = 0;

static const char *get_file_contents(FILE *fp)
{
    static char *buffer = NULL;
    struct stat stat_buf;

    rewind(fp);

    fstat(fileno(fp), &stat_buf);

    buffer = realloc(buffer, stat_buf.st_size + 1);

    if (fread(buffer, 1, stat_buf.st_size, fp) < stat_buf.st_size) {
        return NULL;
    }
    else {
        buffer[stat_buf.st_size] = '\0';

        return buffer;
    }
}

#define TEST_FILE "/tmp/log_test"

static void log_handler(const char *msg, void *udata)
{
    FILE *fp = udata;

    fputs(msg, fp);
}

static void init_logger(FILE **tmp_file)
{
    logger = logCreate();

    debug_stream = logStream(logger, "DEBUG");
    info_stream = logStream(logger, "INFO");

    /* Log DEBUG and INFO messages to the named file. */
    logToFile(logger, debug_stream | info_stream, TEST_FILE);
    /* Log DEBUG messages to the fp of this temp file. */
    logToFP(logger, debug_stream, tmp_file[0] = tmpfile());
    /* Log INFO messages via log_handler to this other temp file. */
    logToFunction(logger, info_stream, log_handler, tmp_file[1] = tmpfile());
}

static void write_messages(void)
{
    _logWrite(logger, debug_stream, "some_file", 1, "some_func", "message 1\n");
    _logWrite(logger, info_stream, "some_file", 2, "some_func", "message 2\n");
    _logWrite(logger, debug_stream | info_stream, "some_file", 3, "some_func", "message 3\n");
}

static void close_logger(FILE **tmp_file)
{
    logClose(logger);

    fclose(tmp_file[0]);
    fclose(tmp_file[1]);

    // remove(TEST_FILE);
}

#define check_fp(fp, expected) _check_fp(fp, expected, __FILE__, __LINE__)

static void _check_fp(FILE *fp, const char *expected, const char *file, int line)
{
    const char *actual = get_file_contents(fp);

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s:%d: Test failed.\n\nExpected:\n\n%s\nGot:\n\n%s",
                file, line, expected, actual);

        errors++;
    }
}

#define check_file(name, expected) _check_file(name, expected, __FILE__, __LINE__)

static void _check_file(const char *filename, const char *text, const char *file, int line)
{
    FILE *fp = fopen(filename, "r");

    _check_fp(fp, text, file, line);

    fclose(fp);
}

int main(int argc, char *argv[])
{
    FILE *tmp_file[2] = { 0 };

    init_logger(tmp_file);
    write_messages();

    check_fp(tmp_file[0],
            "message 1\n"
            "message 3\n");

    check_fp(tmp_file[1],
            "message 2\n"
            "message 3\n");

    check_file(TEST_FILE,
            "message 1\n"
            "message 2\n"
            "message 3\n");

    close_logger(tmp_file);

    init_logger(tmp_file);
    logWithUniversalDate(logger);
    write_messages();

    check_fp(tmp_file[0],
            "1970-01-01 message 1\n"
            "1970-01-01 message 3\n");

    check_fp(tmp_file[1],
            "1970-01-01 message 2\n"
            "1970-01-01 message 3\n");

    check_file(TEST_FILE,
            "1970-01-01 message 1\n"
            "1970-01-01 message 2\n"
            "1970-01-01 message 3\n");

    close_logger(tmp_file);

    init_logger(tmp_file);
    logWithUniversalDate(logger);
    logWithUniversalTime(logger, 3);
    write_messages();

    check_fp(tmp_file[0],
            "1970-01-01 12:00:00.000 message 1\n"
            "1970-01-01 12:00:00.000 message 3\n");

    check_fp(tmp_file[1],
            "1970-01-01 12:00:00.000 message 2\n"
            "1970-01-01 12:00:00.000 message 3\n");

    check_file(TEST_FILE,
            "1970-01-01 12:00:00.000 message 1\n"
            "1970-01-01 12:00:00.000 message 2\n"
            "1970-01-01 12:00:00.000 message 3\n");

    close_logger(tmp_file);

    init_logger(tmp_file);
    logWithUniversalDate(logger);
    logWithUniversalTime(logger, 3);
    logWithFile(logger);
    logWithLine(logger);
    logWithFunction(logger);
    logWithSeparator(logger, "\t");
    write_messages();

    check_fp(tmp_file[0],
            "1970-01-01\t12:00:00.000\tsome_file\t1\tsome_func\tmessage 1\n"
            "1970-01-01\t12:00:00.000\tsome_file\t3\tsome_func\tmessage 3\n");

    check_fp(tmp_file[1],
            "1970-01-01\t12:00:00.000\tsome_file\t2\tsome_func\tmessage 2\n"
            "1970-01-01\t12:00:00.000\tsome_file\t3\tsome_func\tmessage 3\n");

    check_file(TEST_FILE,
            "1970-01-01\t12:00:00.000\tsome_file\t1\tsome_func\tmessage 1\n"
            "1970-01-01\t12:00:00.000\tsome_file\t2\tsome_func\tmessage 2\n"
            "1970-01-01\t12:00:00.000\tsome_file\t3\tsome_func\tmessage 3\n");

    close_logger(tmp_file);

    return errors;
}
#endif
