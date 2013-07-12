#ifndef DP_H
#define DP_H

/*
 * dp.h: Description
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-07-12
 * Version:	$Id$
 */

typedef struct DP_Stream DP_Stream;
typedef struct DP_Object DP_Object;

typedef enum {
    DP_STRING,
    DP_INT,
    DP_FLOAT,
    DP_CONTAINER
} DP_Type;

struct DP_Object {
    DP_Object *next;
    DP_Type type;
    char *name;
    union {
        char      *s;
        long int   i;
        double     f;
        DP_Object *c;
    } u;
};

/*
 * Create and return a DP_Stream, using data from file <filename>.
 */
DP_Stream *dpOpenFile(const char *filename);

/*
 * Create and return a DP_Stream, using data from FILE pointer <fp>.
 */
DP_Stream *dpOpenFP(FILE *fp);

/*
 * Create and return a DP_Stream, using data from file descriptor <fd>.
 */
DP_Stream *dpOpenFD(int fd);

/*
 * Create and return a DP_Stream, using data from string <string>.
 */
DP_Stream *dpOpenString(const char *string);

/*
 * Parse <stream>, returning the first of the found objects.
 */
DP_Object *dpParse(DP_Stream *stream);

/*
 * Retrieve an error text from <stream>, in case any function has returned an error.
 */
const char *dpError(DP_Stream *stream);

/*
 * Free the object list starting at <root>.
 */
void dpFree(DP_Object *root);

/*
 * Free the memory occupied by <stream>.
 */
void dpClose(DP_Stream *stream);

#endif
