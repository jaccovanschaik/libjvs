#ifndef LIBJVS_LOG_H
#define LIBJVS_LOG_H

/*
 * log.h: Write log messages.
 *
 * Part of libjvs.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <syslog.h>

typedef struct Logger Logger;

/*
 * <streams>, in any of the functions below, is a 64-bit bitmask where each bit
 * represents an output stream. You use the streams parameter in the logWrite
 * and logAppend functions to define which streams that particular message goes
 * to, and you use the streams parameter in the logTo... functions to define
 * which streams have their output sent to that destination.
 */

#define logWrite(logger, streams, ...) \
    _logWrite(logger, streams, __FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * Create a new logger.
 */
Logger *logCreate(void);

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
uint64_t logStream(Logger *logger, const char *name);

/*
 * Send logging on any of <streams> of <logger> to a UDP socket on port <port>
 * on host <host>. If the host could not be found, -1 is returned and the stream
 * is not created.
 */
int logToUDP(Logger *logger, uint64_t streams, const char *host, uint16_t port);

/*
 * Send logging on any of <streams> of <logger> through a TCP connection to port
 * <port> on host <host>. If no connection could be opened, -1 is returned and
 * the stream is not created.
 */
int logToTCP(Logger *logger, uint64_t streams, const char *host, uint16_t port);

/*
 * Write logging on any of <streams> of <logger> to the file whose name is
 * specified with <fmt> and the subsequent parameters. If the file could not be
 * opened, -1 is returned and the stream is not created.
 */
int logToFile(Logger *logger, uint64_t streams, const char *fmt, ...);

/*
 * Write logging on any of <streams> of <logger> to the previously opened FILE
 * pointer <fp>.
 */
int logToFP(Logger *logger, uint64_t streams, FILE *fp);

/*
 * Write logging on any of <streams> of <logger> to the previously opened file
 * descriptor <fd>.
 */
int logToFD(Logger *logger, uint64_t streams, int fd);

/*
 * Write logging on any of <streams> of <logger> to the syslog facility using
 * the given parameters (see openlog(3) for the meaning of these parameters).
 */
int logToSyslog(Logger *logger, uint64_t streams, const char *ident, int option, int facility, int priority);

/*
 * Call function <handler> for every log messageof <logger> , passing in the
 * message and the <udata> that is given here.
 */
int logToFunction(Logger *logger, uint64_t streams, void (*handler)(const char *msg, void *udata), void *udata);

/*
 * Add a UTC date of the form YYYY-MM-DD in log messages on <logger>.
 */
void logWithUniversalDate(Logger *logger);

/*
 * Add a UTC timestamp of the form HH:MM:SS to log messages on <logger>.
 * <precision> is the number of sub-second digits to show.
 */
void logWithUniversalTime(Logger *logger, int precision);

/*
 * Add a local date of the form YYYY-MM-DD in log messages on <logger>.
 */
void logWithLocalDate(Logger *logger);

/*
 * Add a local timestamp of the form HH:MM:SS to log messages on <logger>.
 * <precision> is the number of sub-second digits to show.
 */
void logWithLocalTime(Logger *logger, int precision);

/*
 * Add the name of the file from which the logWrite function was called to log
 * messages on <logger>.
 */
void logWithFile(Logger *logger);

/*
 * Add the name of the function that called the logWrite function to log
 * messages on <logger>.
 */
void logWithFunction(Logger *logger);

/*
 * Add the line number of the call to the logWrite function to log message on
 * <logger>.
 */
void logWithLine(Logger *logger);

/*
 * Add a string defined by <fmt> and the subsequent arguments to log messages on
 * <logger>.
 */
void logWithString(Logger *logger, const char *fmt, ...);

/*
 * Use a string defined with <fmt> and the subsequent arguments as a separator
 * between the fields of log messages on <logger> (default is a single space).
 */
void logWithSeparator(Logger *logger, const char *fmt, ...);

/*
 * Send out a logging message using <fmt> and the subsequent parameters to
 * <streams> on <logger>. <file>, <line> and <func> are filled in by the
 * logWrite macro, which should be used to call this function.
 */
void _logWrite(Logger *logger, uint64_t streams,
        const char *file, int line, const char *func, const char *fmt, ...);

/*
 * Send a logging message using <fmt> and the subsequent parameters to <streams>
 * on <logger>, *without* any prefixes. Useful to continue a previous log
 * message.
 */
void logContinue(Logger *logger, uint64_t streams, const char *fmt, ...);

/*
 * Close <logger>: close all outputs, remove all prefixes.
 */
void logClose(Logger *logger);

#ifdef __cplusplus
}
#endif

#endif
