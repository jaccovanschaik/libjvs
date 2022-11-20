#ifndef LIBJVS_MDF_H
#define LIBJVS_MDF_H

/* mdf.h: Minimal Data Format reader.
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
 * as a MDF_Object, and the caller can go to subsequent objects by following a
 * "next" pointer. Contents of the objects are stored in a union based on the
 * type of object described above.
 *
 * mdf.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: mdf.h 467 2022-11-20 00:05:38Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

typedef struct MDF_Stream MDF_Stream;
typedef struct MDF_Object MDF_Object;

/* MDF_Object types. */

typedef enum {
    MDF_STRING,             /* A (double-quoted) string. */
    MDF_INT,                /* A (long) integer. */
    MDF_FLOAT,              /* A (double-precision) float. */
    MDF_CONTAINER           /* A container with more MDF_Objects. */
} MDF_Type;

/* The MDF_Object itself. */

struct MDF_Object {
    MDF_Object *next;       /* Next object in the sequence. */
    MDF_Type    type;       /* Type of the object. */
    char       *name;       /* Name of the object. */
    const char *file;       /* File... */
    int         line;       /* ... and line where the object was found. */
    union {                 /* Type-specific object data. */
        char       *s;      /* Pointer to a string (if type == MDF_STRING) */
        long int    i;      /* An integer (if type == MDF_INT) */
        double      f;      /* A float (if type == MDF_FLOAT) */
        MDF_Object *c;      /* First in a sub-list of objects
                               (if type == MDF_CONTAINER) */
    } u;
};

/*
 * Create and return an MDF_Stream, using data from file <filename>.
 */
MDF_Stream *mdfOpenFile(const char *filename);

/*
 * Create and return an MDF_Stream, using data from FILE pointer <fp>.
 */
MDF_Stream *mdfOpenFP(FILE *fp);

/*
 * Create and return an MDF_Stream, using data from file descriptor <fd>.
 */
MDF_Stream *mdfOpenFD(int fd);

/*
 * Create and return an MDF_Stream, using data from string <string>.
 */
MDF_Stream *mdfOpenString(const char *string);

/*
 * Parse <stream>, returning the first of the found objects.
 */
MDF_Object *mdfParse(MDF_Stream *stream);

/*
 * Return type <type> as a string.
 */
const char *mdfTypeName(const MDF_Type type);

/*
 * Retrieve an error text from <stream>, in case any function has returned an
 * error.
 */
const char *mdfError(const MDF_Stream *stream);

/*
 * Free the object list starting at <root>.
 */
void mdfFree(MDF_Object *root);

/*
 * Close <stream> and free all its memory.
 */
void mdfClose(MDF_Stream *stream);

#ifdef __cplusplus
}
#endif

#endif
