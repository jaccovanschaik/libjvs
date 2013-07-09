#ifndef LOG_H
#define LOG_H

/*
 * log.h: Description
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>

typedef struct Logger Logger;

#define logWrite (_logSetLocation(__FILE__, __LINE__, __func__), _logWrite)

Logger *logCreate(void);

int logToUDP(Logger *logger, const char *host, int port);

int logToTCP(Logger *logger, const char *host, int port);

int logToFile(Logger *logger, const char *filename);

int logToFP(Logger *logger, FILE *fp);

int logToFD(Logger *logger, int fd);

int logToSyslog(Logger *logger, const char *ident, int option, int facility, int priority);

void logWithTime(Logger *logger, int on, int precision);

void logWithDate(Logger *logger, int on);

void logWithFile(Logger *logger, int on);

void logWithFunction(Logger *logger, int on);

void logWithLine(Logger *logger, int on);

void _logSetLocation(const char *file, int line, const char *func);

void _logWrite(Logger *logger, const char *fmt, ...);

void logClose(Logger *logger);

#endif
