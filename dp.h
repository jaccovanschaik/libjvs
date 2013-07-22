#ifndef DP_H
#define DP_H

/* dp.c: Data parser
 *
 * Copyright:   (c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
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
    const char *file;
    int   line;
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
 * Return the type of <obj> as a string.
 */
const char *dpType(const DP_Object *obj);

/*
 * Retrieve an error text from <stream>, in case any function has returned an error.
 */
const char *dpError(const DP_Stream *stream);

/*
 * Free the object list starting at <root>.
 */
void dpFree(DP_Object *root);

/*
 * Free the memory occupied by <stream>.
 */
void dpClose(DP_Stream *stream);

#endif
