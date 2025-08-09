#ifndef LOG_H
#define LOG_H

/*
 * log.h: Provide logging.
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
 * log.h is part of libjvs.
 *
 * Copyright: (c) 2019-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-29
 * Version:   $Id: log.h 504 2025-08-09 15:49:37Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdint.h>
#include <syslog.h>

#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These are some predefined channel masks, based on the log levels in syslog.h.
 * You can use these, or you can define your own. It's entirely up to you. The
 * software doesn't attach any meaning to these numbers, they are just bits
 * representing log channels.
 */

enum {
    CH_NONE    = 0,                 // Don't actually log anything
    CH_EMERG   = 1 << LOG_EMERG,    // system is unusable
    CH_ALERT   = 1 << LOG_ALERT,    // action must be taken immediately
    CH_CRIT    = 1 << LOG_CRIT,     // critical conditions
    CH_ERR     = 1 << LOG_ERR,	    // error conditions
    CH_WARNING = 1 << LOG_WARNING,  // warning conditions
    CH_NOTICE  = 1 << LOG_NOTICE,   // normal but significant condition
    CH_INFO    = 1 << LOG_INFO,     // informational
    CH_DEBUG   = 1 << LOG_DEBUG,    // debug-level messages
};

typedef struct LogWriter LogWriter;

/*
 * Write a log message to <channels>, defined by the printf-compatible format
 * string <fmt> and the subsequent parameters. If necessary, <file>, <line>
 * and <func> will be used to fill the appropriate prefixes. Note that this
 * will not add a newline by default. If you want one, you'll have to add it
 * in <fmt> yourself.
 */
#define logWrite(channels, ...) \
    _logWrite(channels, true, __FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * Continue a previous log message using the printf-compatible format string
 * <fmt> and the subsequent parameters. This does the same as logWrite()
 * above, but it doesn't print any prefixes.
 */
#define logContinue(channels, ...) \
    _logWrite(channels, false, __FILE__, __LINE__, __func__, __VA_ARGS__)

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
LogWriter *logSyslogWriter(const char *ident, int option,
        int facility, int priority);

/*
 * Get a log writer that calls <handler> for every log message, passing in the
 * message as <msg> and the same <udata> that was given here.
 */
LogWriter *logFunctionWriter(void (*handler)(const char *msg, void *udata),
        void *udata);

/*
 * Get a log writer that writes into Buffer <buf>.
 */
LogWriter *logBufferWriter(Buffer *buf);

/*
 * Close down writer <writer>. All channels connected to it are disconnected,
 * and all facilities opened by the writer (files, network connections etc.)
 * are closed.
 */
void logCloseWriter(LogWriter *writer);

/*
 * Separate prefixes in log messages to <writer> with the separator given by
 * <sep>. Separators are *not* written if the prefix before or after it is a
 * string prefix (added using logWithString). The user is probably trying to set
 * up alternative separators between certain prefixes and we don't want to mess
 * that up. The default separator is a single space.
 */
void logWithSeparator(LogWriter *writer, const char *sep);

/*
 * Prefix log messages to <writer> with the local time, formatted using the
 * strftime-compatible format <fmt>. <fmt> supports an additional digit in the
 * %S specifier (i.e. "%<n>S") where <n> is the number of sub-second digits to
 * add after the decimal point. If <n> is left out altogether, the result is
 * identical to the usual "%S" specifier, where the current seconds value is
 * shown. If <n> is 0, the seconds value is rounded to the nearest whole second.
 */
void logWithLocalTime(LogWriter *writer, const char *fmt);

/*
 * Prefix log messages to <writer> with the UTC time, formatted using the
 * strftime-compatible format <fmt>. <fmt> supports an additional digit in the
 * %S specifier (i.e. "%<n>S") where <n> is the number of sub-second digits to
 * add after the decimal point. If <n> is left out altogether, the result is
 * identical to the usual "%S" specifier, where the current seconds value is
 * shown. If <n> is 0, the seconds value is rounded to the nearest whole second.
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
 * Prefix log messages to <writer> with the string defined by the
 * printf-compatible format string <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 2, 3)))
void logWithString(LogWriter *writer, const char *fmt, ...);

/*
 * Prefix log messages to <writer> with the current thread id.
 */
void logWithThreadId(LogWriter *writer);

/*
 * Connect the log channels in <channels> to writer <writer>.
 */
void logConnect(uint64_t channels, LogWriter *writer);

/*
 * Write a log message to <channels>, defined by the printf-compatible
 * format string <fmt> and the subsequent parameters. If <with_prefixes> is
 * true, the message will be preceded by the requested prefixes. <file>,
 * <line> and <func> will be used to fill the appropriate prefixes, if
 * necessary. Note that this will not add a newline by default. If you want
 * one, you'll have to add it in <fmt> yourself.
 *
 * Call this function through the logWrite macro, which will fill in <file>,
 * <line> and <func> for you.
 */
__attribute__((format (printf, 6, 7)))
void _logWrite(uint64_t channels, bool with_prefixes,
        const char *file, int line, const char *func,
        const char *fmt, ...);

/*
 * Reset all logging. Disconnects all channels and deletes all writers.
 */
void logReset(void);

#ifdef __cplusplus
}
#endif

#endif
