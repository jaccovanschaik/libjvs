/*
 * A simple data reader. When a datafile is read using sdrRead, the caller gets
 * a sequence of sdrObject structs to traverse.
 *
 * Copyright:	(c) 2006 Jacco van Schaik (jacco@frontier.nl)
 * Version:	$Id: sdr.h 243 2007-06-24 13:33:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef SDR_H
#define SDR_H

#include <stdio.h>

typedef struct sdrObject sdrObject;

typedef enum {
   SDR_STRING,
   SDR_LONG,
   SDR_DOUBLE,
   SDR_CONTAINER
} sdrObjectType;

struct sdrObject {
   sdrObject *next;
   sdrObject *parent;
   sdrObjectType type;
   char *name;
   char *file;
   int line;
   union {
      char *s;
      long l;
      double d;
      sdrObject *o;
   } data;
};

sdrObject *sdrRead(char *file);

void sdrFree(sdrObject *obj, int clear_string_data);

void sdrDump(FILE *fp, sdrObject *obj, int indent);

#endif
