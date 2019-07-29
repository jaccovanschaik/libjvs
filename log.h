#ifndef LOG_H
#define LOG_H

/*
 * log.h: XXX
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-07-29
 * Version:   $Id: log.h 334 2019-07-29 18:07:55Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define logWrite(channels, ...) \
    _logWrite(channels, __FILE__, __LINE__, __func__, __VA_ARGS__)

typedef struct LogWriter LogWriter;
typedef struct LogChannel LogChannel;

LogWriter *logFileWriter(const char *fmt, ...);

LogWriter *logFPWriter(FILE *fp);

LogWriter *logFDWriter(int fd);

LogWriter *logTCPWriter(const char *host, uint16_t port);

LogWriter *logUDPWriter(const char *host, uint16_t port);

LogWriter *logSyslogWriter(const char *ident, int option, int facility, int priority);

LogWriter *logFunctionWriter(void (*handler)(const char *msg, void *udata), void *udata);

void logWithLocalTime(LogWriter *writer, const char *fmt);

void logWithUniversalTime(LogWriter *writer, const char *fmt);

void logWithFile(LogWriter *writer);

void logWithLine(LogWriter *writer);

void logWithFunction(LogWriter *writer);

void logWithChannel(LogWriter *writer);

void logWithString(LogWriter *writer, const char *fmt, ...);

void logWithSeparator(LogWriter *writer, const char *sep);

void logCloseWriter(LogWriter *writer);

LogChannel *logChannel(const char *name);

void logConnect(LogChannel *chan, LogWriter *writer);

void logCloseChannel(LogChannel *chan);

void _logWrite(LogChannel *chan, const char *file, int line, const char *func, const char *fmt, ...);

void logContinue(LogChannel *chan, const char *fmt, ...);

void logReset(void);

#ifdef __cplusplus
}
#endif

#endif
