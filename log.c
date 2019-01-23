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

typedef struct {
    void (*handler)(const char *msg, void *udata);
    void *udata;
} LOG_FunctionData;

/*
 * An output channel for logging.
 */
typedef struct {
    ListNode _node;
    LOG_OutputType type;
    uint64_t channels;
    union {
        FILE *fp;                   /* For LOG_OT_FILE, LOG_OT_FP */
        int fd;                     /* For LOG_OT_UDP, LOG_OT_TCP, LOG_OT_FD */
        int priority;               /* For LOG_OT_SYSLOG */
        LOG_FunctionData function;  /* For LOG_OT_FUNCTION */
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

static char *separator = NULL;

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
#ifdef TEST
    tv->tv_sec  = 12 * 60 * 60;
    tv->tv_usec = 0;
#else
    gettimeofday(tv, NULL);
#endif

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
 * Call function <handler> for every log message, passing in the message and the
 * <udata> that is given here.
 */
int logToFunction(uint64_t channels, void (*handler)(const char *msg, void *udata), void *udata)
{
    LOG_Output *out = log_create_output(LOG_OT_FUNCTION, channels);

    out->u.function.handler = handler;
    out->u.function.udata = udata;

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
 * Use a string defined with <fmt> and the subsequent arguments as a separator
 * between the fields of log messages (default is a single space).
 */
void logWithSeparator(const char *fmt, ...)
{
    va_list ap;
    Buffer string = { 0 };

    va_start(ap, fmt);
    bufSetV(&string, fmt, ap);
    va_end(ap);

    if (separator != NULL) {
        free(separator);
    }

    separator = bufDetach(&string);
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

    if (separator == NULL) {
        separator = strdup(" ");
    }

    for (pfx = listHead(&prefixes); pfx; pfx = listNext(pfx)) {
        switch(pfx->type) {
        case LOG_PT_DATE:
            if (tv.tv_sec == -1) log_get_time(&tm, &tv);

            bufAddF(&scratch, "%04d-%02d-%02d%s",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, separator);

            break;
        case LOG_PT_TIME:
            if (tv.tv_sec == -1) log_get_time(&tm, &tv);

            bufAddF(&scratch, "%02d:%02d:", tm.tm_hour, tm.tm_min);

            if (pfx->u.precision == 0) {
                bufAddF(&scratch, "%02d%s", tm.tm_sec, separator);
            }
            else {
                double seconds =
                    (double) tm.tm_sec + (double) tv.tv_usec / 1000000.0;

                bufAddF(&scratch, "%0*.*f%s",
                        3 + pfx->u.precision, pfx->u.precision, seconds, separator);
            }

            break;
        case LOG_PT_FILE:
            bufAddF(&scratch, "%s%s", file, separator);
            break;
        case LOG_PT_LINE:
            bufAddF(&scratch, "%d%s", line, separator);
            break;
        case LOG_PT_FUNC:
            bufAddF(&scratch, "%s%s", func, separator);
            break;
        case LOG_PT_STR:
            bufAddF(&scratch, "%s%s", pfx->u.string, separator);
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
        case LOG_OT_FUNCTION:
            out->u.function.handler(bufGet(&scratch), out->u.function.udata);
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
        close(out->u.fd);
        break;
    case LOG_OT_FILE:
        /* Opened as FILE pointers. */
        fclose(out->u.fp);
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
void logContinue(uint64_t channels, const char *fmt, ...)
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

/*
 * Close logging: close all channels, remove all prefixes.
 */
void logClose(void)
{
    LOG_Output *out;
    LOG_Prefix *pfx;

    pthread_mutex_lock(&scratch_mutex);

    bufReset(&scratch);

    pthread_mutex_unlock(&scratch_mutex);

    pthread_mutex_lock(&outputs_mutex);

    while ((out = listRemoveHead(&outputs)) != NULL) {
        log_close_output(out);

        free(out);
    }

    pthread_mutex_unlock(&outputs_mutex);

    pthread_mutex_lock(&prefixes_mutex);

    while ((pfx = listRemoveHead(&prefixes)) != NULL) {
        if (pfx->type == LOG_PT_STR && pfx->u.string != NULL) {
            free(pfx->u.string);
        }

        free(pfx);
    }

    pthread_mutex_unlock(&prefixes_mutex);
}

#ifdef TEST
#include <sys/stat.h>

static int errors = 0;

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
    /* Log DEBUG and INFO messages to the named file. */
    logToFile(CH_DEBUG | CH_INFO, TEST_FILE);
    /* Log DEBUG messages to the fp of this temp file. */
    logToFP(CH_DEBUG, tmp_file[0] = tmpfile());
    /* Log INFO messages via log_handler to this other temp file. */
    logToFunction(CH_INFO, log_handler, tmp_file[1] = tmpfile());
}

static void write_messages(void)
{
    _logWrite(CH_DEBUG, "some_file", 1, "some_func", "message 1\n");
    _logWrite(CH_INFO, "some_file", 2, "some_func", "message 2\n");
    _logWrite(CH_DEBUG + CH_INFO, "some_file", 3, "some_func", "message 3\n");
}

static void close_logger(FILE **tmp_file)
{
    logClose();

    fclose(tmp_file[0]);
    fclose(tmp_file[1]);

    remove(TEST_FILE);
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
    logWithDate();
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
    logWithDate();
    logWithTime(3);
    write_messages();

    check_fp(tmp_file[0],
            "1970-01-01 13:00:00.000 message 1\n"
            "1970-01-01 13:00:00.000 message 3\n");

    check_fp(tmp_file[1],
            "1970-01-01 13:00:00.000 message 2\n"
            "1970-01-01 13:00:00.000 message 3\n");

    check_file(TEST_FILE,
            "1970-01-01 13:00:00.000 message 1\n"
            "1970-01-01 13:00:00.000 message 2\n"
            "1970-01-01 13:00:00.000 message 3\n");

    close_logger(tmp_file);

    init_logger(tmp_file);
    logWithDate();
    logWithTime(3);
    logWithFile();
    logWithLine();
    logWithFunction();
    logWithSeparator("\t");
    write_messages();

    check_fp(tmp_file[0],
            "1970-01-01\t13:00:00.000\tsome_file\t1\tsome_func\tmessage 1\n"
            "1970-01-01\t13:00:00.000\tsome_file\t3\tsome_func\tmessage 3\n");

    check_fp(tmp_file[1],
            "1970-01-01\t13:00:00.000\tsome_file\t2\tsome_func\tmessage 2\n"
            "1970-01-01\t13:00:00.000\tsome_file\t3\tsome_func\tmessage 3\n");

    check_file(TEST_FILE,
            "1970-01-01\t13:00:00.000\tsome_file\t1\tsome_func\tmessage 1\n"
            "1970-01-01\t13:00:00.000\tsome_file\t2\tsome_func\tmessage 2\n"
            "1970-01-01\t13:00:00.000\tsome_file\t3\tsome_func\tmessage 3\n");

    close_logger(tmp_file);

    return errors;
}
#endif
