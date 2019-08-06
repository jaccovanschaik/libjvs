#ifndef LOG_H
#define LOG_H

/*
 * log.h: Provide logging.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-29
 * Version:   $Id: log.h 341 2019-08-06 12:39:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdint.h>

#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define logWrite(channels, ...) \
    _logWrite(channels, __FILE__, __LINE__, __func__, __VA_ARGS__)

typedef struct LogWriter LogWriter;
typedef struct LogChannel LogChannel;

/*
 * Get a log writer that writes to the file whose name is given by <fmt> and the
 * subsequent parameters.
 */
__attribute__((format (printf, 1, 2)))
LogWriter *logFileWriter(const char *fmt, ...);

/*
 * Get a log writer that writes to FILE pointer <fp>.
 */
LogWriter *logFPWriter(FILE *fp);

/*
 * Get a log writer that writes to file descriptor <fd>.
 */
LogWriter *logFDWriter(int fd);

/*
 * Get a log writer that writes to a TCP connection to <port> at <host>.
 */
LogWriter *logTCPWriter(const char *host, uint16_t port);

/*
 * Get a log writer that sends log messages as UDP packets to <port> on <host>.
 */
LogWriter *logUDPWriter(const char *host, uint16_t port);

/*
 * Get a log writer that sends log messages to syslog, using the given
 * parameters. See "man 3 syslog" for more information on them.
 */
LogWriter *logSyslogWriter(const char *ident, int option, int facility, int priority);

/*
 * Get a log writer that calls <handler> for every log message, passing in the
 * message as <msg> and the same <udata> that was given here.
 */
LogWriter *logFunctionWriter(void (*handler)(const char *msg, void *udata), void *udata);

/*
 * Get a log writer that writes into Buffer <buf>.
 */
LogWriter *logBufferWriter(Buffer *buf);

/*
 * Prefix log messages to <writer> with the local time, formatted using the
 * strftime-compatible format <fmt>. <fmt> supports an additional format
 * specifier "%<n>N" which is replaced with <n> sub-second digits (i.e. 2 gives
 * hundredths, 3 gives thousands, 6 gives millionths etc.) The number of digits
 * can be 0 to 9. If <n> is not given, the default is 9.
 */
void logWithLocalTime(LogWriter *writer, const char *fmt);

/*
 * Prefix log messages to <writer> with the UTC time, formatted using the
 * strftime-compatible format <fmt>. <fmt> supports an additional format
 * specifier "%<n>N" which is replaced with <n> sub-second digits (i.e. 2 gives
 * hundredths, 3 gives thousands, 6 gives millionths etc.) The number of digits
 * can be 0 to 9. If <n> is not given, the default is 9.
 */
void logWithUniversalTime(LogWriter *writer, const char *fmt);

/*
 * Prefix log messages to <writer> with the name of the source file in which
 * logWrite was called.
 */
void logWithFile(LogWriter *writer);

/*
 * Prefix log messages to <writer> with the line on which logWrite was called.
 */
void logWithLine(LogWriter *writer);

/*
 * Prefix log messages to <writer> with the name of the function in which
 * logWrite was called.
 */
void logWithFunction(LogWriter *writer);

/*
 * Prefix log messages to <writer> with the name of the channel through which
 * the message is sent.
 */
void logWithChannel(LogWriter *writer);

/*
 * Prefix log messages to <writer> with the string defined by the
 * printf-compatible format string <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 2, 3)))
void logWithString(LogWriter *writer, const char *fmt, ...);

/*
 * Separate prefixes in log messages to <writer> with the separator given by
 * <sep>. Separators are *not* written if the prefix before or after it is a
 * predefined string (added using logWithString). The user may be trying to set
 * up alternative separators between certain prefixes and we don't want to mess
 * that up.
 */
void logWithSeparator(LogWriter *writer, const char *sep);

/*
 * Create a new log channel with the name <name>.
 */
LogChannel *logChannel(const char *name);

/*
 * Connect log channel <chan> to writer <writer>.
 */
void logConnect(LogChannel *chan, LogWriter *writer);

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
        const char *fmt, ...);

/*
 * Continue a previous log message using the printf-compatible format string
 * <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 2, 3)))
void logContinue(LogChannel *chan, const char *fmt, ...);

/*
 * Reset all logging. Deletes all created channels and writers.
 */
void logReset(void);

#ifdef __cplusplus
}
#endif

#endif
