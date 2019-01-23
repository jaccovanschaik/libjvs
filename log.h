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

/*
 * <channels>, in any of the functions below, is a 64-bit bitmask where each bit
 * represents an output channel. You use the channels parameter in the logWrite
 * and logAppend functions to define which channels that particular message goes
 * to, and you use the channels parameter in the logTo... functions to define
 * which channels have their output sent to that destination.
 */

/*
 * These are some predefined channel masks, based on the log levels in syslog.h.
 * You can use these, or you can define your own. It's entirely up to you.
 */

enum {
    CH_EMERG   = 1 << LOG_EMERG,    /* system is unusable */
    CH_ALERT   = 1 << LOG_ALERT,    /* action must be taken immediately */
    CH_CRIT    = 1 << LOG_CRIT,     /* critical conditions */
    CH_ERR     = 1 << LOG_ERR,	    /* error conditions */
    CH_WARNING = 1 << LOG_WARNING,  /* warning conditions */
    CH_NOTICE  = 1 << LOG_NOTICE,   /* normal but significant condition */
    CH_INFO    = 1 << LOG_INFO,     /* informational */
    CH_DEBUG   = 1 << LOG_DEBUG,    /* debug-level messages */
};

#define logWrite(channels, ...) _logWrite(channels, __FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * Send logging on any of <channels> to a UDP socket on port <port> on host
 * <host>. If the host could not be found, -1 is returned and the channel is not
 * created.
 */
int logToUDP(uint64_t channels, const char *host, uint16_t port);

/*
 * Send logging on any of <channels> through a TCP connection to port <port> on
 * host <host>. If no connection could be opened, -1 is returned and the channel
 * is not created.
 */
int logToTCP(uint64_t channels, const char *host, uint16_t port);

/*
 * Write logging on any of <channels> to the file whose name is specified with
 * <fmt> and the subsequent parameters. If the file could not be opened, -1 is
 * returned and the channel is not created.
 */
int logToFile(uint64_t channels, const char *fmt, ...) __attribute__((format (printf, 2, 3)));

/*
 * Write logging on any of <channels> to the previously opened FILE pointer
 * <fp>.
 */
int logToFP(uint64_t channels, FILE *fp);

/*
 * Write logging on any of <channels> to the previously opened file descriptor
 * <fd>.
 */
int logToFD(uint64_t channels, int fd);

/*
 * Write logging on any of <channels> to the syslog facility using the given
 * parameters (see openlog(3) for the meaning of these parameters).
 */
int logToSyslog(uint64_t channels, const char *ident, int option, int facility, int priority);

/*
 * Call function <handler> for every log message, passing in the message and the
 * <udata> that is given here.
 */
int logToFunction(uint64_t channels, void (*handler)(const char *msg, void *udata), void *udata);

/*
 * Add a UTC date of the form YYYY-MM-DD in output messages.
 */
void logWithUniversalDate(void);

/*
 * Add a UTC timestamp of the form HH:MM:SS to output messages. <precision> is
 * the number of sub-second digits to show.
 */
void logWithUniversalTime(int precision);

/*
 * Add a local date of the form YYYY-MM-DD in output messages.
 */
void logWithLocalDate(void);

/*
 * Add a local timestamp of the form HH:MM:SS to output messages. <precision> is
 * the number of sub-second digits to show.
 */
void logWithLocalTime(int precision);

/*
 * Add the name of the file from which the logWrite function was called to log
 * messages.
 */
void logWithFile(void);

/*
 * Add the name of the function that called the logWrite function to log
 * messages.
 */
void logWithFunction(void);

/*
 * Add the line number of the call to the logWrite function to log messages
 */
void logWithLine(void);

/*
 * Add a string defined by <fmt> and the subsequent arguments to log messages.
 */
void logWithString(const char *fmt, ...) __attribute__((format (printf, 1, 2)));

/*
 * Use a string defined with <fmt> and the subsequent arguments as a separator
 * between the fields of log messages (default is a single space).
 */
void logWithSeparator(const char *fmt, ...) __attribute__((format (printf, 1, 2)));

/*
 * Send out a logging message using <fmt> and the subsequent parameters to
 * <channels>. <file>, <line> and <func> are filled in by the logWrite macro,
 * which should be used to call this function.
 */
void _logWrite(uint64_t channels,
        const char *file, int line, const char *func, const char *fmt, ...)
    __attribute__((format (printf, 5, 6)));

/*
 * Send a logging message using <fmt> and the subsequent parameters to
 * <channels>, *without* any prefixes. Useful to continue a previous log
 * message.
 */
void logContinue(uint64_t channels, const char *fmt, ...) __attribute__((format (printf, 2, 3)));

/*
 * Close logging: close all channels, remove all prefixes.
 */
void logClose(void);

#ifdef __cplusplus
}
#endif

#endif
