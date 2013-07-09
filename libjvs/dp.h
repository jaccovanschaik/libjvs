#ifndef DP_H
#define DP_H

/*
 * dp.c: Data Parser.
 *
 * Copyright:   (c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
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

/*
 * Create a Data Parser.
 */
DP_Parser *dpCreate(void);

/*
 * Clear out <parser>. Call this before any of the dpParse functions if you want to re-use an
 * existing parser.
 */
void dpClear(DP_Parser *parser);

/*
 * Free the memory occupied by <parser>.
 */
void dpFree(DP_Parser *parser);

/*
 * Using <parser>, parse the contents of <filename> and append the found objects to <objects>.
 */
int dpParseFile(DP_Parser *parser, const char *filename, List *objects);

/*
 * Using <parser>, parse the contents from <fp> and append the found objects to <objects>.
 */
int dpParseFP(DP_Parser *parser, FILE *fp, List *objects);

/*
 * Using <parser>, parse the contents from <fd> and append the found objects to <objects>.
 */
int dpParseFD(DP_Parser *parser, int fd, List *objects);

/*
 * Using <parser>, parse <string> and append the found objects to <objects>.
 */
int dpParseString(DP_Parser *parser, const char *string, List *objects);

/*
 * Retrieve an error text from <parser>, in case any function has returned an error.
 */
const char *dpError(DP_Parser *parser);

/*
 * Free DP_Object <obj>.
 */
void dpFreeObject(DP_Object *obj);

#endif
