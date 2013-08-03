#ifndef LOG_H
#define LOG_H

/*
 * log.h: Write log messages.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>

typedef struct Logger Logger;

#define logWrite (_logSetLocation(__FILE__, __LINE__, __func__), _logWrite)

/*
 * Create a new logger.
 */
Logger *logCreate(void);

/*
 * Add an output channel that sends messages to a UDP socket on port <port> on host <host>. If the
 * host could not be found, -1 is returned and the channel is not created.
 */
int logToUDP(Logger *logger, const char *host, int port);

/*
 * Add an output channel that sends messages over a TCP connection to port <port> on host <host>. If
 * no connection could be opened, -1 is returned and the channel is not created.
 */
int logToTCP(Logger *logger, const char *host, int port);

/*
 * Add an output channel to file <file>. If the file could not be opened, -1 is returned and the
 * channel is not created.
 */
int logToFile(Logger *logger, const char *filename);

/*
 * Add an output channel to the previously opened FILE pointer <fp>.
 */
int logToFP(Logger *logger, FILE *fp);

/*
 * Add an output channel to the previously opened file descriptor <fd>.
 */
int logToFD(Logger *logger, int fd);

/*
 * Add an output channel to the syslog facility using the given parameters (see openlog(3) for the
 * meaning of these parameters).
 */
int logToSyslog(Logger *logger, const char *ident, int option, int facility, int priority);

/*
 * Add a date of the form YYYY-MM-DD in output messages.
 */
void logWithDate(Logger *logger);

/*
 * Add a timestamp of the form HH:MM:SS to output messages. <precision> is the number of sub-second
 * digits to show.
 */
void logWithTime(Logger *logger, int precision);

/*
 * Add the name of the file from which the logWrite function was called to log messages.
 */
void logWithFile(Logger *logger);

/*
 * Add the name of the function that called the logWrite function to log messages.
 */
void logWithFunction(Logger *logger);

/*
 * Add the line number of the call to the logWrite function to log messages
 */
void logWithLine(Logger *logger);

/*
 * Add string <string> to log messages.
 */
void logWithString(Logger *logger, const char *string);

/*
 * Set _log_file, _log_line and _log_func to the given location. Called as part of the logWrite
 * macro, not to be called on its own.
 */
void _logSetLocation(const char *file, int line, const char *func);

/*
 * Send out a logging message using <fmt> and the subsequent parameters through <logger>. Called as
 * part of the logWrite macro, not to be called on its own.
 */
void _logWrite(Logger *logger, const char *fmt, ...) __attribute__((format (printf, 2, 3)));

/*
 * Log <fmt> and the subsequent parameters through <logger>, *without* any prefixes. Useful to
 * continue a previous log message.
 */
void logAppend(Logger *logger, const char *fmt, ...) __attribute__((format (printf, 2, 3)));

/*
 * Close logger <logger>.
 */
void logClose(Logger *logger);

#endif
