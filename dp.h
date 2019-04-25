#ifndef LIBJVS_DP_H
#define LIBJVS_DP_H

/* dp.c: Data parser
 *
 * A data file consists of a sequence of name/value pairs. Names are unquoted
 * strings, starting with a letter or underscore and followed by any number of
 * letters, underscores or digits. Values are any of the following:
 *
 * - A double-quoted string;
 * - A long integer (Hexadecimal if starting with 0x, octal if starting with 0,
 *   otherwise decimal);
 * - A double-precision float;
 * - A container, started with { and ended with }, containing a new sequence of
 *   name/value pairs.
 *
 * Data to be read can be given as a filename, a file descriptor, a FILE pointer
 * or a character string. The first object in the data is returned to the caller
 * as a DP_Object, and the caller can go to subsequent objects by following a
 * "next" pointer. Contents of the objects are stored in a union based on the
 * type of object described above.
 *
 * Data parser is part of libjvs.
 *
 * Copyright:   (c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

typedef struct DP_Stream DP_Stream;
typedef struct DP_Object DP_Object;

/* DP_Object types. */

typedef enum {
    DP_STRING,      /* A (double-quoted) string. */
    DP_INT,         /* A (long) integer. */
    DP_FLOAT,       /* A (double-precision) float. */
    DP_CONTAINER    /* A container with more DP_Objects. */
} DP_Type;

/* The DP_Object itself. */

struct DP_Object {
    DP_Object *next;    /* Next object in the sequence. */
    DP_Type type;       /* Type of the object. */
    char *name;         /* Name of the object. */
    const char *file;   /* File... */
    int   line;         /* ... and line where the object was found. */
    union {
        char      *s;   /* Pointer to a string (if type == DP_STRING) */
        long int   i;   /* An integer (if type == DP_INT) */
        double     f;   /* A float (if type == DP_FLOAT) */
        DP_Object *c;   /* First in a sub-list of objects (if type == DP_CONTAINER) */
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
 * Return type <type> as a string.
 */
const char *dpTypeName(const DP_Type type);

/*
 * Retrieve an error text from <stream>, in case any function has returned an error.
 */
const char *dpError(const DP_Stream *stream);

/*
 * Free the object list starting at <root>.
 */
void dpFree(DP_Object *root);

/*
 * Close <stream> and free all its memory.
 */
void dpClose(DP_Stream *stream);

#ifdef __cplusplus
}
#endif

#endif
