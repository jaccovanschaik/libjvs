/*
 * log.c: Handle logging.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-26
 * Version:   $Id: log.c 332 2019-07-27 20:59:13Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#define _GNU_SOURCE

#include "log.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>

#include "buffer.h"
#include "list.h"
#include "net.h"
#include "udp.h"
#include "tcp.h"
#include "defs.h"
#include "utils.h"

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
    char *filename;             /* For LOG_OT_FILE */
    char *host;                 /* For LOG_OT_UDP, LOG_OT_TCP */
    uint16_t port;              /* For LOG_OT_TCP, LOG_OT_TCP */
    FILE *fp;                   /* For LOG_OT_FILE, LOG_OT_FP */
    int fd;                     /* For LOG_OT_UDP, LOG_OT_TCP, LOG_OT_FD */
    int priority;               /* For LOG_OT_SYSLOG */
    void (*handler)(const char *msg, void *udata);
    void *udata;                /* For LOG_OT_FUNCTION */
} LOG_Output;

/*
 * A listable reference to an output.
 */
typedef struct {
    ListNode _node;
    LOG_Output *output;
} LOG_OutputRef;

/*
 * Prefix types.
 */
typedef enum {
    LOG_PT_UTIME,
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
    char *str;                  /* For LOG_PT_UTIME, LOG_PT_LTIME, LOG_PT_STR */
} LOG_Prefix;

/*
 * Definition of a log channel.
 */
typedef struct {
    List output_refs;           /* List of references to outputs. */
    List prefixes;              /* List of prefixes. */
    char *separator;            /* Separator for this channel. */
} LOG_Channel;

#define LOG_MAX_CHANNELS (8 * sizeof(uint64_t))

static LOG_Channel log_channel[LOG_MAX_CHANNELS];

static List outputs = { 0 };

static LOG_Prefix *log_add_prefix(LOG_Channel *channel, LOG_PrefixType type)
{
    LOG_Prefix *prefix = calloc(1, sizeof(LOG_Prefix));

    prefix->type = type;

    listAppendTail(&channel->prefixes, prefix);

    return prefix;
}

static void add_multi_prefix(uint64_t channels, LOG_PrefixType type)
{
    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if ((channels & (1L << i)) == 0) continue;

        LOG_Channel *channel = log_channel + i;

        log_add_prefix(channel, type);
    }
}

static LOG_Output *log_add_output(LOG_OutputType type)
{
    LOG_Output *output = calloc(1, sizeof(*output));

    output->type = type;

    listAppendTail(&outputs, output);

    return output;
}

static LOG_Output *log_find_file_output(const char *filename)
{
    LOG_Output *output;

    for (output = listHead(&outputs); output; output = listNext(output)) {
        if (output->type == LOG_OT_FILE && strcmp(output->filename, filename) == 0) break;
    }

    return output;
}

static LOG_Output *log_add_file_output(const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL) {
        return NULL;
    }

    LOG_Output *output = log_add_output(LOG_OT_FILE);

    output->filename = strdup(filename);
    output->fp = fp;

    return output;
}

static LOG_Output *log_find_fp_output(FILE *fp)
{
    LOG_Output *output;

    for (output = listHead(&outputs); output; output = listNext(output)) {
        if (output->type == LOG_OT_FP && output->fp == fp) break;
    }

    return output;
}

static LOG_Output *log_add_fp_output(FILE *fp)
{
    LOG_Output *output = log_add_output(LOG_OT_FP);

    output->fp = fp;

    return output;
}

static LOG_Output *log_find_fd_output(int fd)
{
    LOG_Output *output;

    for (output = listHead(&outputs); output; output = listNext(output)) {
        if (output->type == LOG_OT_FD && output->fd == fd) break;
    }

    return output;
}

static LOG_Output *log_add_fd_output(int fd)
{
    LOG_Output *output = log_add_output(LOG_OT_FD);

    output->fd = fd;

    return output;
}

static LOG_Output *log_add_syslog_output(const char *ident, int option,
        int facility, int priority)
{
    LOG_Output *output = log_add_output(LOG_OT_SYSLOG);

    openlog(ident, option, facility);

    output->priority = priority;

    return output;
}

static LOG_Output *log_find_udp_output(const char *host, uint16_t port)
{
    LOG_Output *output;

    for (output = listHead(&outputs); output; output = listNext(output)) {
        if (output->type == LOG_OT_UDP &&
            strcmp(output->host, host) == 0 &&
            output->port == port) break;
    }

    return output;
}

static LOG_Output *log_add_udp_output(const char *host, uint16_t port)
{
    int fd;

    if ((fd = udpSocket()) < 0 || netConnect(fd, host, port) != 0) {
        return NULL;
    }

    LOG_Output *output = log_add_output(LOG_OT_UDP);

    output->host = strdup(host);
    output->port = port;
    output->fd   = fd;

    return output;
}

static LOG_Output *log_find_tcp_output(const char *host, uint16_t port)
{
    LOG_Output *output;

    for (output = listHead(&outputs); output; output = listNext(output)) {
        if (output->type == LOG_OT_TCP &&
            strcmp(output->host, host) == 0 &&
            output->port == port) break;
    }

    return output;
}

static LOG_Output *log_add_tcp_output(const char *host, uint16_t port)
{
    int fd;

    if ((fd = tcpConnect(host, port)) < 0) {
        return NULL;
    }

    LOG_Output *output = log_add_output(LOG_OT_TCP);

    output->host = strdup(host);
    output->port = port;
    output->fd   = fd;

    return output;
}

static LOG_Output *log_find_function_output(
        void (*handler)(const char *msg, void *udata),
        void *udata)
{
    LOG_Output *output;

    for (output = listHead(&outputs); output; output = listNext(output)) {
        if (output->type == LOG_OT_FUNCTION &&
            output->handler == handler &&
            output->udata == udata) break;
    }

    return output;
}

static LOG_Output *log_add_function_output(
        void (*handler)(const char *msg, void *udata),
        void *udata)
{
    LOG_Output *output = log_add_output(LOG_OT_FUNCTION);

    output->handler = handler;
    output->udata = udata;

    return output;
}

static LOG_OutputRef *log_create_output_ref(LOG_Output *output)
{
    LOG_OutputRef *ref = calloc(1, sizeof(*ref));

    ref->output = output;

    return ref;
}

static void log_add_output_ref(LOG_Channel *channel, LOG_Output *output)
{
    LOG_OutputRef *ref = log_create_output_ref(output);

    listAppendTail(&channel->output_refs, ref);
}

/*
 * Fill <tm> and <tv> with the current time.
 */
static void log_get_time(struct timeval *tv)
{
#ifdef TEST
    tv->tv_sec  = 12 * 3600 + 34 * 60 + 56;
    tv->tv_usec = 123456;
#else
    gettimeofday(tv, NULL);
#endif
}

/*
 * Add the time in <tm> and <tv>, using strftime-compatible format <fmt>, to <buf>.
 */
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

/*
 * Add the requested prefixes to <buf>.
 */
static void log_add_prefixes(LOG_Channel *channel, Buffer *buf,
        const char *file, int line, const char *func)
{
    struct tm tm_local = { 0 }, tm_utc = { 0 };
    static struct timeval tv = { 0 };

    LOG_Prefix *pfx, *next_pfx;

    tv.tv_sec = -1;
    tm_utc.tm_sec = -1;
    tm_local.tm_sec = -1;

    if (channel->separator == NULL) {
        channel->separator = strdup(" ");
    }

    for (pfx = listHead(&channel->prefixes); pfx; pfx = next_pfx) {
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
        case LOG_PT_STR:
            bufAddF(buf, "%s", pfx->str);
            break;
        }

        if (pfx->type != LOG_PT_STR &&
                (next_pfx == NULL || next_pfx->type != LOG_PT_STR)) {
            bufAddS(buf, channel->separator);
        }
    }
}

static void log_write(LOG_Channel *channel, const char *buf)
{
    LOG_OutputRef *ref;
    size_t len = strlen(buf);

    for (ref = listHead(&channel->output_refs); ref; ref = listNext(ref)) {
        switch(ref->output->type) {
        case LOG_OT_UDP:
        case LOG_OT_TCP:
        case LOG_OT_FD:
            tcpWrite(ref->output->fd, buf, len);
            break;
        case LOG_OT_FILE:
        case LOG_OT_FP:
            fwrite(buf, len, 1, ref->output->fp);
            break;
        case LOG_OT_SYSLOG:
            syslog(ref->output->priority, "%s", buf);
            break;
        case LOG_OT_FUNCTION:
            ref->output->handler(buf, ref->output->udata);
            break;
        }
    }
}

/*
 * Add a UTC date/time to all log messages on <channels>, using strftime(3)
 * compatible format string <fmt>. This function accepts an additional
 * conversion specifier "%<n>N" which inserts <n> sub-second digits, up to a
 * maximum of 6.
 */
void logWithUniversalTime(uint64_t channels, const char *fmt)
{
    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if ((channels & (1L << i)) == 0) continue;

        LOG_Channel *channel = log_channel + i;

        LOG_Prefix *prefix = log_add_prefix(channel, LOG_PT_UTIME);

        prefix->str = strdup(fmt);
    }
}

/*
 * Add a local date/time to all log messages on <channels>, using strftime(3)
 * compatible format string <fmt>. This function accepts an additional
 * conversion specifier "%<n>N" which inserts <n> sub-second digits, up to a
 * maximum of 6.
 */
void logWithLocalTime(uint64_t channels, const char *fmt, int precision)
{
    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if ((channels & (1L << i)) == 0) continue;

        LOG_Channel *channel = log_channel + i;

        LOG_Prefix *prefix = log_add_prefix(channel, LOG_PT_LTIME);

        prefix->str = strdup(fmt);
    }
}

/*
 * Add the source file from which logWrite is called to all log messages on
 * <channels>.
 */
void logWithFile(uint64_t channels)
{
    add_multi_prefix(channels, LOG_PT_FILE);
}

/*
 * Add the function from which logWrite is called to all log messages on
 * <channels>.
 */
void logWithFunction(uint64_t channels)
{
    add_multi_prefix(channels, LOG_PT_FUNC);
}

/*
 * Add the source line from which logWrite is called to all log messages on
 * <channels>.
 */
void logWithLine(uint64_t channels)
{
    add_multi_prefix(channels, LOG_PT_LINE);
}

/*
 * Add the string given by the printf(3)-compatible format string <fmt> and the
 * subsequent parameters to all log messages on <channels>.
 */
void logWithString(uint64_t channels, const char *fmt, ...)
{
    va_list ap;
    char *buf;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&buf, fmt, ap);
    va_end(ap);

    if (r == -1) {
        return;
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if ((channels & (1L << i)) == 0) continue;

        LOG_Channel *channel = log_channel + i;

        LOG_Prefix *prefix = log_add_prefix(channel, LOG_PT_STR);

        prefix->str = strdup(buf);
    }
}

/*
 * Use string <str> as the separator on all log messages on <channels>. This
 * separator is *not* included when the prefix on either side of it is specified
 * using logWithString() above.
 */
void logWithSeparator(uint64_t channels, const char *str)
{
    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if ((channels & (1L << i)) == 0) continue;

        LOG_Channel *channel = log_channel + i;

        channel->separator = strdup(str);
    }
}

/*
 * Write all log messages on <channels> to the file whose path is given by <fmt>
 * and the subsequent parameters.
 */
int logToFile(uint64_t channels, const char *fmt, ...)
{
    va_list ap;
    char *filename;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&filename, fmt, ap);
    va_end(ap);

    if (r == -1) {
        return -1;
    }

    LOG_Output *output;

    if ((output = log_find_file_output(filename)) == NULL &&
        (output = log_add_file_output(filename)) == NULL) {
        free(filename);
        return -1;
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_add_output_ref(channel, output);
        }
    }

    return 0;
}

/*
 * Write all log messages on <channels> to the open file stream <fp>.
 */
int logToFP(uint64_t channels, FILE *fp)
{
    LOG_Output *output;

    if ((output = log_find_fp_output(fp)) == NULL) {
        output = log_add_fp_output(fp);
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_add_output_ref(channel, output);
        }
    }

    return 0;
}

/*
 * Write all log messages on <channels> to the open file descriptor <fd>.
 */
int logToFD(uint64_t channels, int fd)
{
    LOG_Output *output;

    if ((output = log_find_fd_output(fd)) == NULL) {
        output = log_add_fd_output(fd);
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_add_output_ref(channel, output);
        }
    }

    return 0;
}

/*
 * Write all log messages on <channels> to the syslog facility using the given
 * parameters (see openlog(3) for the meaning of these parameters).
 */
int logToSyslog(uint64_t channels, const char *ident, int option, int facility, int priority)
{
    LOG_Output *output = log_add_syslog_output(ident, option, facility, priority);

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_add_output_ref(channel, output);
        }
    }

    return 0;
}

/*
 * Write all log messages on <channels> as UDP packets to port <port> on host
 * <host>. If the host could not be found, -1 is returned and no packets will be
 * sent.
 */
int logToUDP(uint64_t channels, const char *host, uint16_t port)
{
    LOG_Output *output;

    if ((output = log_find_udp_output(host, port)) == NULL &&
        (output = log_add_udp_output(host, port)) == NULL) {
        return 1;
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_add_output_ref(channel, output);
        }
    }

    return 0;
}

/*
 * Send all log messages on <channels> via a TCP connection to port <port> on
 * host <host>. If a connection could not be opened, -1 is returned and no
 * messages will be sent.
 */
int logToTCP(uint64_t channels, const char *host, uint16_t port)
{
    LOG_Output *output;

    if ((output = log_find_tcp_output(host, port)) == NULL &&
        (output = log_add_tcp_output(host, port)) == NULL) {
        return 1;
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_add_output_ref(channel, output);
        }
    }

    return 0;
}

/*
 * Call function <handler> for every log message on <channels>. The text of the
 * message will be passed in through <msg>, together with the channels through
 * which the message was sent and the udata pointer given here.
 */
int logToFunction(uint64_t channels,
        void (*handler)(const char *msg, void *udata),
        void *udata)
{
    LOG_Output *output;

    if ((output = log_find_function_output(handler, udata)) == NULL &&
        (output = log_add_function_output(handler, udata)) == NULL) {
        return 1;
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_add_output_ref(channel, output);
        }
    }

    return 0;
}

/*
 * Send the message given using <fmt> and the subsequent parameters to
 * <channels>. Call this function using the logWrite macro which will fill in
 * the <file>, <line> and <func> fields for you.
 */
void _logWrite(uint64_t channels, const char *file, int line, const char *func,
        const char *fmt, ...)
{
    va_list ap;

    Buffer msg = { 0 };

    va_start(ap, fmt);
    bufAddV(&msg, fmt, ap);
    va_end(ap);

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            Buffer buf = { 0 };

            log_add_prefixes(channel, &buf, file, line, func);

            bufAdd(&buf, bufGet(&msg), bufLen(&msg));

            log_write(channel, bufGet(&buf));

            bufClear(&buf);
        }
    }

    bufClear(&msg);
}

/*
 * Output a string given by <fmt> and the subsequent parameters to <channels>,
 * but *without* prefixes. Useful to continue a previously started log message.
 */
void logContinue(uint64_t channels, const char *fmt, ...)
{
    char *buf;
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&buf, fmt, ap);
    va_end(ap);

    if (r < 0) {
        return;
    }

    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        if (channels & (1L << i)) {
            LOG_Channel *channel = log_channel + i;

            log_write(channel, buf);
        }
    }

    free(buf);
}

/*
 * Reset logging. Closes all outputs, deletes all channels. After this, you may
 * re-initialize logging using logWith... and logTo... calls.
 */
void logReset(void)
{
    for (int i = 0; i < LOG_MAX_CHANNELS; i++) {
        LOG_Channel *channel = log_channel + i;

        LOG_Prefix *pfx;
        LOG_OutputRef *ref;

        while ((pfx = listRemoveHead(&channel->prefixes)) != NULL) {
            free(pfx->str);
            free(pfx);
        }

        while ((ref = listRemoveHead(&channel->output_refs)) != NULL) {
            free(ref);
        }

        free(channel->separator);
    }

    LOG_Output *out;

    while ((out = listRemoveHead(&outputs)) != NULL) {
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

        free(out->filename);
        free(out->host);

        free(out);
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

    logWithUniversalTime(CH_INFO | CH_DEBUG, "%Y-%m-%d %H:%M:%S.%3N");
    logWithFunction(CH_DEBUG);
    logWithString(CH_DEBUG, "@");
    logWithFile(CH_DEBUG);
    logWithString(CH_DEBUG, ":");
    logWithLine(CH_DEBUG);

    logToFile(CH_INFO | CH_DEBUG, "/tmp/log_test.txt");

    logWrite(CH_INFO, "Logging to CH_INFO\n");
    logWrite(CH_DEBUG, "Logging to CH_DEBUG\n");

    logReset();

    return errors;
}

#endif
