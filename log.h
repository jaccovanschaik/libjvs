#ifndef LOG_H
#define LOG_H

/*
 * log.h: Handle logging.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-26
 * Version:   $Id: log.h 332 2019-07-27 20:59:13Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdint.h>
#include <syslog.h>

/*
 * The functions in this module fall into four categories:
 *
 * - The logWith... functions define prefixes that are added to log messages
 *   (they are added in the order these functions are called);
 * - The logTo... functions define where log messages are written;
 * - The logWrite and LogContinue messages actually write log messages;
 * - The logReset function resets the logging module to its initial state.
 *
 * All of these functions (except for logReset) accept a <channels> parameter
 * that specifies the channel(s) that the function applies to. It is a 64-bit
 * bitmask where each bit represents an output channel. In all of these
 * functions you can bitwise-or together multiple channel masks to have the
 * function apply to multiple log channels. Since there are 64 bits in these
 * bitmasks it follows that you can have a maximum of 64 log channels.
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

/*
 * Add a UTC date/time to all log messages on <channels>, using strftime(3)
 * compatible format string <fmt>. This function accepts an additional
 * conversion specifier "%<n>N" which inserts <n> sub-second digits, up to a
 * maximum of 6.
 */
void logWithUniversalTime(uint64_t channels, const char *fmt);

/*
 * Add a local date/time to all log messages on <channels>, using strftime(3)
 * compatible format string <fmt>. This function accepts an additional
 * conversion specifier "%<n>N" which inserts <n> sub-second digits, up to a
 * maximum of 6.
 */
void logWithLocalTime(uint64_t channels, const char *fmt, int precision);

/*
 * Add the source file from which logWrite is called to all log messages on
 * <channels>.
 */
void logWithFile(uint64_t channels);

/*
 * Add the function from which logWrite is called to all log messages on
 * <channels>.
 */
void logWithFunction(uint64_t channels);

/*
 * Add the source line from which logWrite is called to all log messages on
 * <channels>.
 */
void logWithLine(uint64_t channels);

/*
 * Add the string given by the printf(3)-compatible format string <fmt> and the
 * subsequent parameters to all log messages on <channels>.
 */
void logWithString(uint64_t channels, const char *fmt, ...);

/*
 * Use string <str> as the separator on all log messages on <channels>. This
 * separator is *not* included when the prefix on either side of it is specified
 * using logWithString() above.
 */
void logWithSeparator(uint64_t channels, const char *str);

/*
 * Write all log messages on <channels> to the file whose path is given by <fmt>
 * and the subsequent parameters.
 */
int logToFile(uint64_t channels, const char *fmt, ...);

/*
 * Write all log messages on <channels> to the open file stream <fp>.
 */
int logToFP(uint64_t channels, FILE *fp);

/*
 * Write all log messages on <channels> to the open file descriptor <fd>.
 */
int logToFD(uint64_t channels, int fd);

/*
 * Write all log messages on <channels> to the syslog facility using the given
 * parameters (see openlog(3) for the meaning of these parameters).
 */
int logToSyslog(uint64_t channels, const char *ident, int option, int facility, int priority);

/*
 * Write all log messages on <channels> as UDP packets to port <port> on host
 * <host>. If the host could not be found, -1 is returned and no packets will be
 * sent.
 */
int logToUDP(uint64_t channels, const char *host, uint16_t port);

/*
 * Send all log messages on <channels> via a TCP connection to port <port> on
 * host <host>. If a connection could not be opened, -1 is returned and no
 * messages will be sent.
 */
int logToTCP(uint64_t channels, const char *host, uint16_t port);

/*
 * Call function <handler> for every log message on <channels>. The text of the
 * message will be passed in through <msg>, together with the udata pointer
 * given here.
 */
int logToFunction(uint64_t channels,
        void (*handler)(const char *msg, void *udata),
        void *udata);

/*
 * Send the message given using <fmt> and the subsequent parameters to
 * <channels>. Call this function using the logWrite macro which will fill in
 * the <file>, <line> and <func> fields for you.
 */
void _logWrite(uint64_t channels, const char *file, int line, const char *func,
        const char *fmt, ...);

#define logWrite(channels, ...) \
    _logWrite(channels, __FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * Output a string given by <fmt> and the subsequent parameters to <channels>,
 * but *without* prefixes. Useful to continue a previously started log message.
 */
void logContinue(uint64_t channels, const char *fmt, ...);

/*
 * Reset logging. Closes all outputs, deletes all channels. After this, you may
 * re-initialize logging using logWith... and logTo... calls.
 */
void logReset(void);

#endif
