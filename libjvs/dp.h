#ifndef DP_H
#define DP_H

/*
 * dp.h: Description
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-07-08
 * Version:	$Id$
 */

#include <stdio.h>

#include "list.h"

typedef enum {
    DP_STRING,
    DP_INT,
    DP_FLOAT,
    DP_CONTAINER
} DP_Type;

typedef struct {
    ListNode _node;
    DP_Type type;
    char *name;
    int line;
    union {
        char       *s;
        long int    i;
        double      f;
        List        c;
    } u;
} DP_Object;

typedef struct DP_Parser DP_Parser;

DP_Parser *dpCreate(void);

void dpClear(DP_Parser *parser);

void dpFree(DP_Parser *parser);

int dpParseFile(DP_Parser *parser, const char *filename, List *objects);

int dpParseFP(DP_Parser *parser, FILE *fp, List *objects);

int dpParseFD(DP_Parser *parser, int fd, List *objects);

int dpParseString(DP_Parser *parser, const char *string, List *objects);

const char *dpError(DP_Parser *parser);

void dpFreeObject(DP_Object *obj);

#endif
